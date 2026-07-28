add_executable(
    dc_core_tests
    core_tests.cpp
)

add_executable(
    dc_video_snapshot_tests
    video_snapshot_tests.cpp
)

target_link_libraries(
    dc_core_tests
    PRIVATE
        DevilsCrush::Core
        DevilsCrush::Emulation
        DevilsCrush::GameLogic
)

target_link_libraries(
    dc_video_snapshot_tests
    PRIVATE
        DevilsCrush::Core
        DevilsCrush::Emulation
)

target_compile_features(dc_core_tests PRIVATE cxx_std_20)
dc_enable_strict_warnings(dc_core_tests)
target_compile_features(dc_video_snapshot_tests PRIVATE cxx_std_20)
dc_enable_strict_warnings(dc_video_snapshot_tests)

add_test(NAME dc_core_tests COMMAND dc_core_tests)

add_test(
    NAME dc_coverage_manifest
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/Verify-CoverageManifest.ps1"
        -ManifestPath
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/COVERAGE_MANIFEST.md"
        -TestListPath
        "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
)

if(TARGET devils_crush)
    add_test(NAME dc_cli_help COMMAND devils_crush --help)
    add_test(
        NAME dc_cli_missing_rom
        COMMAND devils_crush --validate-rom
                "${CMAKE_CURRENT_BINARY_DIR}/intentionally-missing.pce"
    )
    set_tests_properties(dc_cli_missing_rom PROPERTIES WILL_FAIL TRUE)

    add_test(NAME dc_cli_unknown_option COMMAND devils_crush --not-an-option)
    set_tests_properties(dc_cli_unknown_option PROPERTIES WILL_FAIL TRUE)

    add_test(
        NAME dc_cli_orphan_canonical_work_ram
        COMMAND devils_crush --canonical-work-ram unused.bin
    )
    set_tests_properties(
        dc_cli_orphan_canonical_work_ram
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_orphan_canonical_framebuffer
        COMMAND devils_crush --canonical-framebuffer unused.rgb
    )
    set_tests_properties(
        dc_cli_orphan_canonical_framebuffer
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_orphan_canonical_audio
        COMMAND devils_crush --canonical-audio unused.wav
    )
    set_tests_properties(
        dc_cli_orphan_canonical_audio
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_orphan_pass_through_hook
        COMMAND devils_crush --pass-through-hook 00:E462
    )
    set_tests_properties(
        dc_cli_orphan_pass_through_hook
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_orphan_observe_hook
        COMMAND devils_crush --observe-hook 00:E462
    )
    set_tests_properties(
        dc_cli_orphan_observe_hook
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_orphan_routine_trace
        COMMAND devils_crush --routine-trace unused.csv
    )
    set_tests_properties(
        dc_cli_orphan_routine_trace
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_orphan_routine_call_trace
        COMMAND devils_crush --routine-call-trace unused.csv
    )
    set_tests_properties(
        dc_cli_orphan_routine_call_trace
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_orphan_routine_replay
        COMMAND devils_crush --routine-replay 00:F1B4@1
    )
    set_tests_properties(
        dc_cli_orphan_routine_replay
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_invalid_routine_replay
        COMMAND
            devils_crush
            --record-gameplay
            unused.csv
            --observe-hook
            00:F1B4
            --routine-call-trace
            unused-calls.csv
            --routine-replay
            00:F1B4@0
    )
    set_tests_properties(
        dc_cli_invalid_routine_replay
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_unobserved_routine_replay
        COMMAND
            devils_crush
            --record-gameplay
            unused.csv
            --observe-hook
            00:F1B4
            --routine-call-trace
            unused-calls.csv
            --routine-replay
            01:DD09@1
    )
    set_tests_properties(
        dc_cli_unobserved_routine_replay
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_duplicate_routine_replay
        COMMAND
            devils_crush
            --record-gameplay
            unused.csv
            --observe-hook
            00:F1B4
            --routine-call-trace
            unused-calls.csv
            --routine-replay
            00:F1B4@1
            --routine-replay
            00:F1B4@2
    )
    set_tests_properties(
        dc_cli_duplicate_routine_replay
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_orphan_work_ram_patch
        COMMAND devils_crush --work-ram-patch 1:3000:80
    )
    set_tests_properties(
        dc_cli_orphan_work_ram_patch
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_invalid_work_ram_patch
        COMMAND
            devils_crush
            --record-state-census
            unused.csv
            --work-ram-patch
            1:4000:80
    )
    set_tests_properties(
        dc_cli_invalid_work_ram_patch
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_out_of_range_work_ram_patch
        COMMAND
            devils_crush
            --record-state-census
            unused.csv
            --work-ram-patch
            3601:3000:80
    )
    set_tests_properties(
        dc_cli_out_of_range_work_ram_patch
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_duplicate_work_ram_patch
        COMMAND
            devils_crush
            --record-state-census
            unused.csv
            --work-ram-patch
            1:3000:80
            --work-ram-patch
            1:3000:81
    )
    set_tests_properties(
        dc_cli_duplicate_work_ram_patch
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_orphan_work_ram_write_range
        COMMAND devils_crush --work-ram-write-range 3000-30FF
    )
    set_tests_properties(
        dc_cli_orphan_work_ram_write_range
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_orphan_work_ram_write_trace
        COMMAND devils_crush --work-ram-write-trace unused.csv
    )
    set_tests_properties(
        dc_cli_orphan_work_ram_write_trace
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_invalid_work_ram_write_range
        COMMAND
            devils_crush
            --record-gameplay
            unused.csv
            --work-ram-write-range
            3100-3000
            --work-ram-write-trace
            unused-writes.csv
    )
    set_tests_properties(
        dc_cli_invalid_work_ram_write_range
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_duplicate_execution_hook
        COMMAND
            devils_crush
            --record-canonical
            unused.csv
            --observe-hook
            00:E195
            --observe-hook
            00:E195
            --routine-trace
            unused-trace.csv
    )
    set_tests_properties(
        dc_cli_duplicate_execution_hook
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_conflicting_recordings
        COMMAND
            devils_crush
            --record-canonical
            canonical.csv
            --record-gameplay
            gameplay.csv
    )
    set_tests_properties(
        dc_cli_conflicting_recordings
        PROPERTIES WILL_FAIL TRUE
    )

    if(DC_ENABLE_ROM_TESTS)
        if(NOT DC_ROM_PATH OR NOT IS_ABSOLUTE "${DC_ROM_PATH}")
            message(
                FATAL_ERROR
                "DC_ENABLE_ROM_TESTS requires an absolute DC_ROM_PATH."
            )
        endif()
        if(NOT EXISTS "${DC_ROM_PATH}")
            message(
                FATAL_ERROR
                "DC_ROM_PATH does not exist: ${DC_ROM_PATH}"
            )
        endif()

        add_test(
            NAME dc_rom_video_snapshot
            COMMAND dc_video_snapshot_tests "${DC_ROM_PATH}"
        )
        set_tests_properties(
            dc_rom_video_snapshot
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_video_row_writer
            CONFIGURATIONS Release
            COMMAND
                dc_video_snapshot_tests
                "${DC_ROM_PATH}"
                --map-row-writer
        )
        set_tests_properties(
            dc_rom_video_row_writer
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_smoke
            COMMAND devils_crush --smoke-test --rom "${DC_ROM_PATH}"
        )
        set_tests_properties(dc_rom_smoke PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_canonical
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/canonical-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_canonical.cmake"
        )
        set_tests_properties(dc_rom_canonical PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_work_ram_patch
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/work-ram-patch-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_work_ram_patch.cmake"
        )
        set_tests_properties(
            dc_rom_work_ram_patch
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_backup_reload
            COMMAND
                devils_crush
                --backup-ram-test
                "${CMAKE_CURRENT_BINARY_DIR}/backup-ram-test.sav"
                --rom
                "${DC_ROM_PATH}"
        )
        set_tests_properties(
            dc_rom_backup_reload
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_gameplay
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/gameplay-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_gameplay.cmake"
        )
        set_tests_properties(dc_rom_gameplay PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_physics
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/physics-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_physics.cmake"
        )
        set_tests_properties(dc_rom_physics PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_controls_physics
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/controls-physics-$<CONFIG>-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_controls_physics.cmake"
        )
        set_tests_properties(
            dc_rom_controls_physics
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_boundary
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-boundary-$<CONFIG>-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_boundary.cmake"
        )
        set_tests_properties(
            dc_rom_collision_boundary
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_session_scoring
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-scoring-$<CONFIG>-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_scoring.cmake"
        )
        set_tests_properties(
            dc_rom_session_scoring
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_state_census
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/state-census-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_state_census.cmake"
        )
        set_tests_properties(
            dc_rom_state_census
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_score
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/score-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_score.cmake"
        )
        set_tests_properties(dc_rom_score PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_rng.cmake"
        )
        set_tests_properties(dc_rom_rng PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_actor_activation
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/actor-activation-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_actor_activation.cmake"
        )
        set_tests_properties(
            dc_rom_actor_activation
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_actor_layout
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/actor-layout-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_actor_layout.cmake"
        )
        set_tests_properties(
            dc_rom_actor_layout
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_scene_layout
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/scene-layout-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_scene_layout.cmake"
        )
        set_tests_properties(
            dc_rom_scene_layout
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_selective_scene
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/selective-scene-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_selective_scene.cmake"
        )
        set_tests_properties(
            dc_rom_selective_scene
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_return
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-return-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_return.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_return
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_layout
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-layout-v1"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_layout.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_layout
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_audio_command
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/audio-command-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_audio_command.cmake"
        )
        set_tests_properties(
            dc_rom_audio_command
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_two_hit_target
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/two-hit-target-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_two_hit_target.cmake"
        )
        set_tests_properties(
            dc_rom_two_hit_target
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_branching_two_hit
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/branching-two-hit-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_branching_two_hit.cmake"
        )
        set_tests_properties(
            dc_rom_branching_two_hit
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tower_family
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-family-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_family.cmake"
        )
        set_tests_properties(
            dc_rom_tower_family
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bumper_family
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bumper-family-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bumper_family.cmake"
        )
        set_tests_properties(
            dc_rom_bumper_family
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_front_spider
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/front-spider-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_front_spider.cmake"
        )
        set_tests_properties(
            dc_rom_front_spider
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_devil_shooter
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devil-shooter-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devil_shooter.cmake"
        )
        set_tests_properties(
            dc_rom_devil_shooter
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_devil_shooter_companions
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devil-shooter-companions-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devil_shooter_companions.cmake"
        )
        set_tests_properties(
            dc_rom_devil_shooter_companions
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_devils_coffin
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devils-coffin-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devils_coffin.cmake"
        )
        set_tests_properties(
            dc_rom_devils_coffin
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_dragon_giant_skeleton
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-giant-skeleton-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_dragon_giant_skeleton.cmake"
        )
        set_tests_properties(
            dc_rom_dragon_giant_skeleton
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs_controller
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-controller-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs_controller.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs_controller
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs_reset_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-reset-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs_reset_rng.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs_reset_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_gray_blobs
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/gray-blobs-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_gray_blobs.cmake"
        )
        set_tests_properties(
            dc_rom_gray_blobs
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_gray_blob_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/gray-blob-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_gray_blob_rng.cmake"
        )
        set_tests_properties(
            dc_rom_gray_blob_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_impact_burst
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/impact-burst-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_impact_burst.cmake"
        )
        set_tests_properties(
            dc_rom_impact_burst
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_dragon_eggs
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-eggs-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_dragon_eggs.cmake"
        )
        set_tests_properties(
            dc_rom_dragon_eggs
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_skeleton_insects
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/skeleton-insects-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_skeleton_insects.cmake"
        )
        set_tests_properties(
            dc_rom_skeleton_insects
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_sorcerers
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sorcerers-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_sorcerers.cmake"
        )
        set_tests_properties(
            dc_rom_sorcerers
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_sorcerer_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sorcerer-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_sorcerer_rng.cmake"
        )
        set_tests_properties(
            dc_rom_sorcerer_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_small_soldier_sources
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/small-soldier-sources-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_small_soldier_sources.cmake"
        )
        set_tests_properties(
            dc_rom_small_soldier_sources
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face_helmet
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-helmet-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_helmet.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_helmet
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_four_star_wall_pockets
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/four-star-wall-pockets-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_four_star_wall_pockets.cmake"
        )
        set_tests_properties(
            dc_rom_four_star_wall_pockets
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_upper_wall_pocket_hands
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/upper-wall-pocket-hands-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_upper_wall_pocket_hands.cmake"
        )
        set_tests_properties(
            dc_rom_upper_wall_pocket_hands
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_upper_wall_pocket_hand_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/upper-wall-pocket-hand-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_upper_wall_pocket_hand_rng.cmake"
        )
        set_tests_properties(
            dc_rom_upper_wall_pocket_hand_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_upper_wall_pocket_hand_response_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/upper-wall-pocket-hand-response-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_upper_wall_pocket_hand_response_rng.cmake"
        )
        set_tests_properties(
            dc_rom_upper_wall_pocket_hand_response_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_feature_network
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-feature-network-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_feature_network.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_feature_network
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_little_skull_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/little-skull-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_little_skull_rng.cmake"
        )
        set_tests_properties(
            dc_rom_little_skull_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_undead_head_selector_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/undead-head-selector-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_undead_head_selector_rng.cmake"
        )
        set_tests_properties(
            dc_rom_undead_head_selector_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_fixed_system_controllers
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fixed-system-controllers-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_fixed_system_controllers.cmake"
        )
        set_tests_properties(
            dc_rom_fixed_system_controllers
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knights
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knights-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knights.cmake"
        )
        set_tests_properties(
            dc_rom_knights
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_walled_star
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/walled-star-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_walled_star.cmake"
        )
        set_tests_properties(
            dc_rom_walled_star
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_walled_star_access
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/walled-star-access-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_walled_star_access.cmake"
        )
        set_tests_properties(
            dc_rom_walled_star_access
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_side_star_access
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/side-star-access-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_side_star_access.cmake"
        )
        set_tests_properties(
            dc_rom_side_star_access
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_skeleton_eyeball
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-skeleton-eyeball-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_skeleton_eyeball.cmake"
        )
        set_tests_properties(
            dc_rom_giant_skeleton_eyeball
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_mercy_stars
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/mercy-stars-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_mercy_stars.cmake"
        )
        set_tests_properties(
            dc_rom_mercy_stars
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_side_spiders
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/side-spiders-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_side_spiders.cmake"
        )
        set_tests_properties(
            dc_rom_side_spiders
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tower_guard
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-guard-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_guard.cmake"
        )
        set_tests_properties(
            dc_rom_tower_guard
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage1
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage1-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage1.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage1
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage1_results
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage1-results-v1"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage1_results.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage1_results
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage1_events
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage1-events-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage1_events.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage1_events
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage1_natural_perfect
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage1-natural-perfect-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage1_natural_perfect.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage1_natural_perfect
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage2_targets
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage2-targets-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage2_targets.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage2_targets
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage2_resolution
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage2-resolution-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage2_resolution.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage2_resolution
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage2_contact_perfect
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage2-contact-perfect-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage2_contact_perfect.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage2_contact_perfect
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage2_full_contact_perfect
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage2-full-contact-perfect-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage2_full_contact_perfect.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage2_full_contact_perfect
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage3
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage3-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage3.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage3
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage3_natural_perfect
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage3-natural-perfect-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage3_natural_perfect.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage3_natural_perfect
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage4
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage4-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage4.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage4
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage4_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage4-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage4_rng.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage4_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage4_natural_perfect
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage4-natural-perfect-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage4_natural_perfect.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage4_natural_perfect
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage5
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage5-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage5.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage5
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage6
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage6-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage6.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage6
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage6_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage6-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage6_rng.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage6_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage6_natural_perfect
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage6-natural-perfect-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage6_natural_perfect.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage6_natural_perfect
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_stage5_natural_perfect
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-stage5-natural-perfect-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_stage5_natural_perfect.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_stage5_natural_perfect
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bonus_scored_results
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bonus-scored-results-v1"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bonus_scored_results.cmake"
        )
        set_tests_properties(
            dc_rom_bonus_scored_results
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_maximum_score_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/maximum-score-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_maximum_score_rng.cmake"
        )
        set_tests_properties(
            dc_rom_maximum_score_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_remaining_normal_rng
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/remaining-normal-rng-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_remaining_normal_rng.cmake"
        )
        set_tests_properties(
            dc_rom_remaining_normal_rng
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_native_replacement
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/native-replacement-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_native_replacement.cmake"
        )
        set_tests_properties(
            dc_rom_native_replacement
            PROPERTIES LABELS rom_required
        )
    endif()
endif()
