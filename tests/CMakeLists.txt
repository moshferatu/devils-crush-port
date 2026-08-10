add_executable(
    dc_core_tests
    core_tests.cpp
)

add_executable(
    dc_video_snapshot_tests
    video_snapshot_tests.cpp
)

add_executable(
    dc_title_logo_tests
    title_logo_tests.cpp
)

target_link_libraries(
    dc_core_tests
    PRIVATE
        DevilsCrush::Core
        DevilsCrush::Emulation
        DevilsCrush::GameLogic
        DevilsCrush::NativeRegistry
)

target_link_libraries(
    dc_video_snapshot_tests
    PRIVATE
        DevilsCrush::Core
        DevilsCrush::Emulation
)

target_link_libraries(
    dc_title_logo_tests
    PRIVATE
        DevilsCrush::Core
)

target_compile_features(dc_core_tests PRIVATE cxx_std_20)
dc_enable_strict_warnings(dc_core_tests)
target_compile_features(dc_video_snapshot_tests PRIVATE cxx_std_20)
dc_enable_strict_warnings(dc_video_snapshot_tests)
target_compile_features(dc_title_logo_tests PRIVATE cxx_std_20)
dc_enable_strict_warnings(dc_title_logo_tests)

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

add_test(
    NAME dc_native_registry_manifest
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/Generate-NativeRegistry.ps1"
        -RepositoryRoot
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        -Check
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
        NAME dc_cli_unproven_native_replacement
        COMMAND
            devils_crush
            --record-gameplay
            unused.csv
            --replace-hook
            00:F1B4@2
    )
    set_tests_properties(
        dc_cli_unproven_native_replacement
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_unproven_native_damping_replacement
        COMMAND
            devils_crush
            --record-gameplay
            unused.csv
            --replace-hook
            01:DD09@3
    )
    set_tests_properties(
        dc_cli_unproven_native_damping_replacement
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_unproven_native_rotation_replacement
        COMMAND
            devils_crush
            --record-gameplay
            unused.csv
            --replace-hook
            01:D1A2@1
    )
    set_tests_properties(
        dc_cli_unproven_native_rotation_replacement
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_unproven_native_collision_response_replacement
        COMMAND
            devils_crush
            --record-state-census
            unused.csv
            --replace-hook
            03:7FB1@2
    )
    set_tests_properties(
        dc_cli_unproven_native_collision_response_replacement
        PROPERTIES WILL_FAIL TRUE
    )

    add_test(
        NAME dc_cli_unproven_native_collision_table_replacement
        COMMAND
            devils_crush
            --record-gameplay
            unused.csv
            --replace-hook
            03:6966@11
    )
    set_tests_properties(
        dc_cli_unproven_native_collision_table_replacement
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
            NAME dc_rom_title_logo
            COMMAND dc_title_logo_tests "${DC_ROM_PATH}"
        )
        set_tests_properties(
            dc_rom_title_logo
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
            NAME dc_rom_cabinet_smoke
            COMMAND
                devils_crush
                --cabinet-smoke-test
                --rom
                "${DC_ROM_PATH}"
        )
        set_tests_properties(
            dc_rom_cabinet_smoke
            PROPERTIES
                LABELS rom_required
                ENVIRONMENT
                    "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
        )

        add_test(
            NAME dc_rom_host_registry
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                -P
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_host_registry.cmake"
        )
        set_tests_properties(
            dc_rom_host_registry
            PROPERTIES
                LABELS rom_required
                ENVIRONMENT
                    "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
        )

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
            NAME dc_rom_flipper_input_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/flipper-input-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_flipper_input_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_flipper_input_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_scan_vertical_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-scan-vertical-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_scan_vertical_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_collision_scan_vertical_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_geometry_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-geometry-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_geometry_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_collision_geometry_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_mask_lookup_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-mask-lookup-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_mask_lookup_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_collision_mask_lookup_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_flipper_phase_step_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/flipper-phase-step-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_flipper_phase_step_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_flipper_phase_step_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_flipper_geometry_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/flipper-geometry-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_flipper_geometry_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_flipper_geometry_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_signed_word_negation_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/signed-word-negation-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_signed_word_negation_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_signed_word_negation_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_unsigned_byte_multiply_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/unsigned-byte-multiply-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_unsigned_byte_multiply_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_unsigned_byte_multiply_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_unsigned_multiply_round_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/unsigned-multiply-round-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_unsigned_multiply_round_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_unsigned_multiply_round_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_signed_fixed_multiply_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/signed-fixed-multiply-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_signed_fixed_multiply_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_signed_fixed_multiply_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_flipper_projection_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/flipper-projection-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_flipper_projection_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_flipper_projection_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_motion_rotation_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/motion-rotation-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_motion_rotation_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_motion_rotation_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_signed_division_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/signed-division-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_signed_division_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_signed_division_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_motion_damping_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/motion-damping-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_motion_damping_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_motion_damping_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_flipper_radius_projection_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/flipper-radius-projection-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_flipper_radius_projection_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_flipper_radius_projection_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_flipper_angle_projection_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/flipper-angle-projection-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_flipper_angle_projection_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_flipper_angle_projection_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_motion_adjustment_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-motion-adjustment-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_motion_adjustment_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_collision_motion_adjustment_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_coroutine_scheduler_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/coroutine-scheduler-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_coroutine_scheduler_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_coroutine_scheduler_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_nudge_input_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/nudge-input-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_nudge_input_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_nudge_input_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_session_input_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-input-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_input_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_session_input_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_skeleton_eyeball_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-skeleton-eyeball-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_skeleton_eyeball_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_skeleton_eyeball_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_skeleton_eyeball_opening_wait_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-skeleton-eyeball-opening-wait-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_skeleton_eyeball_opening_wait_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_skeleton_eyeball_opening_wait_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_skeleton_eyeball_steady_idle_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-skeleton-eyeball-steady-idle-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_skeleton_eyeball_steady_idle_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_skeleton_eyeball_steady_idle_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_skeleton_eyeball_final_wait_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-skeleton-eyeball-final-wait-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_skeleton_eyeball_final_wait_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_skeleton_eyeball_final_wait_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_gray_blob_initializer_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/gray-blob-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_gray_blob_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_gray_blob_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_gray_blob_first_live_prefix_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/gray-blob-first-live-prefix-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_gray_blob_first_live_prefix_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_gray_blob_first_live_prefix_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_mercy_star_initializer_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/mercy-star-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_mercy_star_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_mercy_star_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_mercy_star_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/mercy-star-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_mercy_star_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_mercy_star_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs_controller_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-controller-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs_controller_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs_controller_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs_appearance_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-appearance-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs_appearance_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs_appearance_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_dragon_egg_controller_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-egg-controller-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_dragon_egg_controller_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_dragon_egg_controller_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_dragon_egg_hit_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-egg-hit-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_dragon_egg_hit_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_dragon_egg_hit_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_dragon_egg_hit_continuations_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-egg-hit-continuations-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_dragon_egg_hit_continuations_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_dragon_egg_hit_continuations_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_feature_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-feature-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_feature_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_feature_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_gate_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_left_phase_four_semantic
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-left-phase-four-semantic-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_left_phase_four_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_left_phase_four_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_left_phase_five_semantic
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-left-phase-five-semantic-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_left_phase_five_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_left_phase_five_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_left_phase_six_semantic
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-left-phase-six-semantic-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_left_phase_six_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_left_phase_six_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_left_lower_reset_semantic
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-left-lower-reset-semantic-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_left_lower_reset_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_left_lower_reset_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_left_lower_equal_semantic
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-left-lower-equal-semantic-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_left_lower_equal_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_left_lower_equal_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_right_helper_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-right-helper-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_right_helper_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_right_helper_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_right_phase_two_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-right-phase-two-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_right_phase_two_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_right_phase_two_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_right_phase_one_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-right-phase-one-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_right_phase_one_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_right_phase_one_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_right_phase_zero_semantic
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-right-phase-zero-semantic-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_right_phase_zero_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_right_phase_zero_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_right_phase_four_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-right-phase-four-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_right_phase_four_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_right_phase_four_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_pending_right_phase_four_equal_semantic
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-pending-right-phase-four-equal-semantic-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_pending_right_phase_four_equal_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_pending_right_phase_four_equal_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tower_insect_source_countdown_wait_semantic
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-insect-source-countdown-wait-semantic-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_insect_source_countdown_wait_semantic.cmake"
        )
        set_tests_properties(dc_rom_tower_insect_source_countdown_wait_semantic PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_devil_shooter_companion_dormant_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devil-shooter-companion-dormant-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devil_shooter_companion_dormant_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_devil_shooter_companion_dormant_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_devil_shooter_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devil-shooter-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devil_shooter_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_devil_shooter_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_dragon_head_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-head-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_dragon_head_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_dragon_head_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_skeleton_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-skeleton-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_skeleton_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_skeleton_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs_controller_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-controller-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs_controller_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs_controller_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_devil_shooter_hit_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devil-shooter-hit-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devil_shooter_hit_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_devil_shooter_hit_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_devil_shooter_catch_continuation_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devil-shooter-catch-continuation-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devil_shooter_catch_continuation_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_devil_shooter_catch_continuation_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_skeleton_insect_left_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/skeleton-insect-left-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_skeleton_insect_left_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_skeleton_insect_left_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_skeleton_insect_right_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/skeleton-insect-right-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_skeleton_insect_right_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_skeleton_insect_right_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_skeleton_insect_center_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/skeleton-insect-center-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_skeleton_insect_center_gate_semantic.cmake"
        )
          set_tests_properties(
              dc_rom_skeleton_insect_center_gate_semantic
              PROPERTIES LABELS rom_required
          )

          add_test(
              NAME dc_rom_skeleton_insect_center_continuation_semantic
              COMMAND
                  "${CMAKE_COMMAND}"
                  "-DAPP=$<TARGET_FILE:devils_crush>"
                  "-DCONFIG=$<CONFIG>"
                  "-DROM=${DC_ROM_PATH}"
                  "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/skeleton-insect-center-continuation-semantic-v1.csv"
                  "-P"
                  "${CMAKE_CURRENT_SOURCE_DIR}/verify_skeleton_insect_center_continuation_semantic.cmake"
          )
          set_tests_properties(
              dc_rom_skeleton_insect_center_continuation_semantic
              PROPERTIES LABELS rom_required
          )

          add_test(
            NAME dc_rom_devils_coffin_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devils-coffin-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devils_coffin_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_devils_coffin_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_devils_coffin_opening_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devils-coffin-opening-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devils_coffin_opening_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_devils_coffin_opening_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_devils_coffin_closing_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devils-coffin-closing-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devils_coffin_closing_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_devils_coffin_closing_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_dragon_head_hit_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-head-hit-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_dragon_head_hit_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_dragon_head_hit_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_dragon_head_hit_continuation_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-head-hit-continuation-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_dragon_head_hit_continuation_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_dragon_head_hit_continuation_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_walled_star_hit_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/walled-star-hit-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_walled_star_hit_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_walled_star_hit_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_walled_star_access_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/walled-star-access-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_walled_star_access_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_walled_star_access_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_scan_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-scan-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_scan_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_collision_scan_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_actor_scan_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/actor-scan-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_actor_scan_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_actor_scan_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_actor_dispatch_vector_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/actor-dispatch-vector-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_actor_dispatch_vector_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_actor_dispatch_vector_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_actor_queue_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/actor-queue-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_actor_queue_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_actor_queue_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_actor_scan_advance_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/actor-scan-advance-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_actor_scan_advance_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_actor_scan_advance_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_actor_camera_sweep_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/actor-camera-sweep-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_actor_camera_sweep_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_actor_camera_sweep_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_sound_timer_driver_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sound-timer-driver-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_sound_timer_driver_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_sound_timer_driver_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_music_sequencer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/music-sequencer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_music_sequencer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_music_sequencer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_palette_shadow_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/palette-shadow-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_palette_shadow_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_palette_shadow_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bank_call_wrappers_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bank-call-wrappers-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bank_call_wrappers_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_bank_call_wrappers_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_vblank_handler_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/vblank-handler-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_vblank_handler_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_vblank_handler_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_vram_upload_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/vram-upload-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_vram_upload_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_vram_upload_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tilemap_upload_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tilemap-upload-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tilemap_upload_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_tilemap_upload_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_tail_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-tail-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_tail_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_collision_tail_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_sorcerer_tick_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sorcerer-tick-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_sorcerer_tick_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_sorcerer_tick_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_actor_geometry_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/actor-geometry-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_actor_geometry_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_actor_geometry_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_actor_loop_linkage_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/actor-loop-linkage-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_actor_loop_linkage_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_actor_loop_linkage_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_scan_advance_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-scan-advance-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_scan_advance_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_collision_scan_advance_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_sat_tail_clear_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sat-tail-clear-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_sat_tail_clear_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_sat_tail_clear_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_sat_submission_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sat-submission-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_sat_submission_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_sat_submission_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_sprite_emit_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sprite-emit-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_sprite_emit_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_sprite_emit_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_side_star_initializers_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/side-star-initializers-semantic-v1"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_side_star_initializers_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_side_star_initializers_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_initializers_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-initializers-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_initializers_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_knight_initializers_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_side_spider_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/side-spider-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_side_spider_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_side_spider_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_front_spider_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/front-spider-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_front_spider_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_front_spider_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tower_hit_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-hit-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_hit_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_tower_hit_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_hit_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-hit-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_hit_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_knight_hit_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_hit_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-hit-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_hit_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_hit_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_score_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-score-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_score_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_score_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_acknowledgement_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-acknowledgement-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_acknowledgement_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_acknowledgement_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_scheduler
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-scheduler-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_scheduler.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_scheduler
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_setup_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-setup-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_setup_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_setup_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_table_setup_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-table-setup-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_table_setup_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_table_setup_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_call_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-call-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_call_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_call_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_pointer_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-pointer-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_pointer_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_pointer_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_shared_call_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-shared-call-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_shared_call_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_shared_call_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_mode_branch_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-mode-branch-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_mode_branch_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_mode_branch_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_position_carry_clear_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-carry-clear-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_carry_clear_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_position_carry_clear_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_position_base_load_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-base-load-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_base_load_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_position_base_load_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_position_mode_add_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-mode-add-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_mode_add_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_position_mode_add_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_position_store_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-store-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_store_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_position_store_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_position_high_load_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-high-load-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_high_load_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_position_high_load_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_position_high_add_prefix
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-high-add-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_high_add_prefix.cmake"
        )
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_position_high_add_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_position_high_store_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-high-store-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_high_store_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_position_high_store_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_position_high_scratch_clear_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-high-scratch-clear-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_high_scratch_clear_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_position_high_scratch_clear_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_position_high_mode_load_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-high-mode-load-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_high_mode_load_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_position_high_mode_load_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_position_high_mode_signed_branch_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-high-mode-signed-branch-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_high_mode_signed_branch_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_position_high_mode_signed_branch_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_position_high_carry_clear_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-high-carry-clear-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_high_carry_clear_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_position_high_carry_clear_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_position_high_base_load_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-high-base-load-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_high_base_load_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_position_high_base_load_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_position_high_scratch_add_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-high-scratch-add-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_high_scratch_add_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_position_high_scratch_add_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_position_high_actor_store_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-position-high-actor-store-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_position_high_actor_store_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_position_high_actor_store_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_field_load_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-field-load-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_field_load_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_field_load_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_field_scratch_add_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-field-scratch-add-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_field_scratch_add_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_field_scratch_add_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_field_store_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-field-store-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_field_store_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_field_store_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_field_clear_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-field-clear-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_field_clear_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_field_clear_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_field_zero_store_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-field-zero-store-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_field_zero_store_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_field_zero_store_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_field_second_zero_store_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-field-second-zero-store-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_field_second_zero_store_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_field_second_zero_store_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_attribute_load_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-attribute-load-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_attribute_load_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_attribute_load_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_attribute_mask_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-attribute-mask-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_attribute_mask_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_attribute_mask_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_attribute_mode_merge_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-attribute-mode-merge-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_attribute_mode_merge_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_attribute_mode_merge_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_attribute_store_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-attribute-store-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_attribute_store_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_attribute_store_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_field_constant_load_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-field-constant-load-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_field_constant_load_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_field_constant_load_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_field_constant_store_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-field-constant-store-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_field_constant_store_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_field_constant_store_prefix PROPERTIES LABELS rom_required)

        add_test(NAME dc_rom_knight_terminal_release_helper_post_return_actor_field_second_constant_load_prefix CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>" "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-actor-field-second-constant-load-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_actor_field_second_constant_load_prefix.cmake")
        set_tests_properties(dc_rom_knight_terminal_release_helper_post_return_actor_field_second_constant_load_prefix PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_pointer_low_load_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-pointer-low-load-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_pointer_low_load_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_pointer_low_load_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_pointer_low_store_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-pointer-low-store-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_pointer_low_store_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_pointer_low_store_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_pointer_high_load_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-pointer-high-load-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_pointer_high_load_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_pointer_high_load_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_pointer_high_store_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-pointer-high-store-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_pointer_high_store_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_pointer_high_store_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_scratch_clear_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-scratch-clear-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_scratch_clear_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_scratch_clear_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_post_return_mode_load_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-post-return-mode-load-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_post_return_mode_load_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_post_return_mode_load_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_shared_jump_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-shared-jump-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_shared_jump_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_shared_jump_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_shared_scan_initializer_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-shared-scan-initializer-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_shared_scan_initializer_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_shared_scan_initializer_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_shared_scan_read_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-shared-scan-read-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_shared_scan_read_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_shared_scan_read_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_shared_scan_nonzero_branch_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-shared-scan-nonzero-branch-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_shared_scan_nonzero_branch_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_shared_scan_nonzero_branch_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_shared_scan_increment_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-shared-scan-increment-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_shared_scan_increment_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_shared_scan_increment_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_shared_scan_range_compare_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-shared-scan-range-compare-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_shared_scan_range_compare_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_shared_scan_range_compare_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_shared_scan_loop_branch_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-shared-scan-loop-branch-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_shared_scan_loop_branch_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_shared_scan_loop_branch_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_allocator_load_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-allocator-load-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_allocator_load_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_allocator_load_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_allocator_store_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-allocator-store-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_allocator_store_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_allocator_store_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_allocator_mode_load_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-allocator-mode-load-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_allocator_mode_load_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_allocator_mode_load_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_allocator_mode_branch_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-allocator-mode-branch-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_allocator_mode_branch_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_allocator_mode_branch_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_helper_allocator_return_carry_clear_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-helper-allocator-return-carry-clear-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_helper_allocator_return_carry_clear_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_helper_allocator_return_carry_clear_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_terminal_release_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-terminal-release-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_terminal_release_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_knight_terminal_release_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_waiting_parent_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-waiting-parent-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_waiting_parent_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_knight_waiting_parent_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_release_transition
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-release-transition-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_release_transition.cmake"
        )
        set_tests_properties(
            dc_rom_knight_release_transition
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_movement_parent_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-movement-parent-semantic-v1.csv"
                "-DORDINAL=2"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_movement_parent_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_knight_movement_parent_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_movement_parent_timer_expiry_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-movement-parent-timer-expiry-semantic-v1.csv"
                "-DORDINAL=4"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_movement_parent_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_knight_movement_parent_timer_expiry_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_movement_parent_phase_wrap_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-movement-parent-phase-wrap-semantic-v1.csv"
                "-DORDINAL=16"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_movement_parent_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_knight_movement_parent_phase_wrap_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_movement_parent_interrupt_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-movement-parent-interrupt-semantic-v1.csv"
                "-DORDINAL=58"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_movement_parent_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_knight_movement_parent_interrupt_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_movement_parent_collision_return_interrupt_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-movement-parent-collision-return-interrupt-semantic-v1.csv"
                "-DORDINAL=60"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_movement_parent_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_knight_movement_parent_collision_return_interrupt_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_knight_movement_parent_all_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/knight-movement-parent-all-semantic-v1.csv"
                "-DORDINAL=all"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_knight_movement_parent_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_knight_movement_parent_all_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_spider_appearance_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/spider-appearance-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_spider_appearance_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_spider_appearance_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tower_insect_animation_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-insect-animation-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_insect_animation_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_tower_insect_animation_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tower_insect_source_gate_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-insect-source-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_insect_source_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_tower_insect_source_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tower_insect_native_family_debug
            CONFIGURATIONS Debug
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-insect-native-family-debug-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_insect_native_family_debug.cmake"
        )
        set_tests_properties(
            dc_rom_tower_insect_native_family_debug
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_small_soldier_source_gates_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/small-soldier-source-gates-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_small_soldier_source_gates_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_small_soldier_source_gates_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_sorcerer_color_gate_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sorcerer-color-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_sorcerer_color_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_sorcerer_color_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_sorcerer_live_controller_gate_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sorcerer-live-controller-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_sorcerer_live_controller_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_sorcerer_live_controller_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_little_skull_gray_release_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/little-skull-gray-release-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_little_skull_gray_release_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_little_skull_gray_release_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_little_skull_live_continuation_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/little-skull-live-continuation-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_little_skull_live_continuation_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_little_skull_live_continuation_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tower_hit_continuations_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-hit-continuations-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_hit_continuations_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_tower_hit_continuations_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tower_guard_selector_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-guard-selector-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_guard_selector_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_tower_guard_selector_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bumper_hit_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bumper-hit-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bumper_hit_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_bumper_hit_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_bumper_hit_continuations_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bumper-hit-continuations-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bumper_hit_continuations_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_bumper_hit_continuations_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_fixed_response_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fixed-response-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_fixed_response_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_fixed_response_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_fixed_scheduler_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fixed-scheduler-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_fixed_scheduler_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_fixed_scheduler_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_fixed_scheduler_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fixed-scheduler-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_fixed_scheduler_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_fixed_scheduler_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_fixed_marker_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fixed-marker-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_fixed_marker_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_fixed_marker_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_fixed_response_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fixed-response-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_fixed_response_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_fixed_response_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_fixed_marker_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fixed-marker-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_fixed_marker_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_fixed_marker_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_scaled_word_add_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/scaled-word-add-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_scaled_word_add_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_scaled_word_add_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door_hit_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-hit-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door_hit_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door_hit_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_walled_star_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/walled-star-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_walled_star_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_walled_star_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face_request_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-request-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_request_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_request_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face_animator_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-animator-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_animator_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_animator_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face_animator_active_wait_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-animator-active-wait-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_animator_active_wait_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_animator_active_wait_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face_crown_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-crown-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_crown_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_crown_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face_mouth_gate_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-mouth-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_mouth_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_mouth_gate_semantic
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
            NAME dc_rom_collision_table_pass
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-pass-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_pass.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_pass
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_pass_second
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-pass-second-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_pass_second.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_pass_second
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_pass_third
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-pass-third-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_pass_third.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_pass_third
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_pass_fourth
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-pass-fourth-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_pass_fourth.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_pass_fourth
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_pass_alternate
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-pass-alternate-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_pass_alternate.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_pass_alternate
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_pass_repeat
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-pass-repeat-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_pass_repeat.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_pass_repeat
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_pass_repeat_second
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-pass-repeat-second-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_pass_repeat_second.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_pass_repeat_second
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_pass_repeat_third
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-pass-repeat-third-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_pass_repeat_third.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_pass_repeat_third
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_pass_repeat_fourth
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-pass-repeat-fourth-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_pass_repeat_fourth.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_pass_repeat_fourth
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_continuation
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-continuation-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_continuation.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_continuation
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_continuation_second
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-continuation-second-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_continuation_second.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_continuation_second
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_continuation_alternate
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-continuation-alternate-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_continuation_alternate.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_continuation_alternate
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_continuation_horizontal
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-continuation-horizontal-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_continuation_horizontal.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_continuation_horizontal
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_continuation_reverse
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-continuation-reverse-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_continuation_reverse.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_continuation_reverse
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_continuation_vertical
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-continuation-vertical-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_continuation_vertical.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_continuation_vertical
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_continuation_vertical_second
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-continuation-vertical-second-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_continuation_vertical_second.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_continuation_vertical_second
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_reverse
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-reverse-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_reverse.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_reverse
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_alternate_reverse
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-alternate-reverse-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_alternate_reverse.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_alternate_reverse
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_vertical
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-vertical-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_vertical.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_vertical
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_table_record_vertical_second
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-table-record-vertical-second-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_table_record_vertical_second.cmake"
        )
        set_tests_properties(
            dc_rom_collision_table_record_vertical_second
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_response_reload
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-response-reload-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_response_reload.cmake"
        )
        set_tests_properties(
            dc_rom_collision_response_reload
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_response_followup
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-response-followup-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_response_followup.cmake"
        )
        set_tests_properties(
            dc_rom_collision_response_followup
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_response_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-response-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_response_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_collision_response_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_collision_consumer_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/collision-consumer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_collision_consumer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_collision_consumer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_motion_damping_nonzero
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/motion-damping-nonzero-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_motion_damping_nonzero.cmake"
        )
        set_tests_properties(
            dc_rom_motion_damping_nonzero
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_motion_damping_negative
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/motion-damping-negative-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_motion_damping_negative.cmake"
        )
        set_tests_properties(
            dc_rom_motion_damping_negative
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_position_integration_positive
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/position-integration-positive-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_position_integration_positive.cmake"
        )
        set_tests_properties(
            dc_rom_position_integration_positive
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_position_integration_integer_step
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/position-integration-integer-step-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_position_integration_integer_step.cmake"
        )
        set_tests_properties(
            dc_rom_position_integration_integer_step
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_position_integration_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/position-integration-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_position_integration_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_position_integration_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_cached_position_restore_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cached-position-restore-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_cached_position_restore_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_cached_position_restore_semantic
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
            NAME dc_rom_ball_drain_threshold_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ball-drain-threshold-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_ball_drain_threshold_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_ball_drain_threshold_semantic
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
            NAME dc_rom_actor_slot_reset_semantic
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/actor-slot-reset-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_actor_slot_reset_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_actor_slot_reset_semantic
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
            NAME dc_rom_bumper_terminal
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bumper-terminal-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_bumper_terminal.cmake"
        )
        set_tests_properties(
            dc_rom_bumper_terminal
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
            NAME dc_rom_giant_face_helmet_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-helmet-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_helmet_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_helmet_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face_helmet_outer_initializers_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-helmet-outer-initializers-semantic-v1"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_helmet_outer_initializers_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_helmet_outer_initializers_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face_helmet_hit_continuation_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-helmet-hit-continuation-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_helmet_hit_continuation_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_helmet_hit_continuation_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face_right_helmet_hit_continuation_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-right-helmet-hit-continuation-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_right_helmet_hit_continuation_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_right_helmet_hit_continuation_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_face_helmet_followup_timer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-face-helmet-followup-timer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_face_helmet_followup_timer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_giant_face_helmet_followup_timer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_little_skull_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/little-skull-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_little_skull_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_little_skull_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_little_skull_pair_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/little-skull-pair-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_little_skull_pair_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_little_skull_pair_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_little_skull_hidden_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/little-skull-hidden-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_little_skull_hidden_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_little_skull_hidden_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_big_star_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/big-star-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_big_star_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_big_star_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_dragon_egg_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-egg-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_dragon_egg_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_dragon_egg_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_dragon_egg_controller_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/dragon-egg-controller-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_dragon_egg_controller_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_dragon_egg_controller_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs_first_movement_prefix_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-first-movement-prefix-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs_first_movement_prefix_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs_first_movement_prefix_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs_shared_motion_idle_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-shared-motion-idle-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs_shared_motion_idle_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs_shared_motion_idle_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs_direct_continuation_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-direct-continuation-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs_direct_continuation_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs_direct_continuation_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_three_blobs_collision_gate_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/three-blobs-collision-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_three_blobs_collision_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_three_blobs_collision_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_undead_head_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/undead-head-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_undead_head_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_undead_head_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_undead_head_selector_gate_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/undead-head-selector-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_undead_head_selector_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_undead_head_selector_gate_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_undead_head_waiting_parent_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/undead-head-waiting-parent-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_undead_head_waiting_parent_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_undead_head_waiting_parent_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_side_worm_waiting_parent_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/side-worm-waiting-parent-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_side_worm_waiting_parent_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_side_worm_waiting_parent_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_devils_coffin_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/devils-coffin-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_devils_coffin_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_devils_coffin_initializer_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_side_worm_initializers_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/side-worm-initializers-semantic-v1"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_side_worm_initializers_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_side_worm_initializers_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_side_worm_selector_gate_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/side-worm-selector-gate-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_side_worm_selector_gate_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_side_worm_selector_gate_semantic
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
            NAME dc_rom_upper_wall_pocket_hand_expiry
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/upper-wall-pocket-hand-expiry-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_upper_wall_pocket_hand_expiry.cmake"
        )
        set_tests_properties(
            dc_rom_upper_wall_pocket_hand_expiry
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_upper_wall_pocket_hand_opening
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/upper-wall-pocket-hand-opening-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_upper_wall_pocket_hand_opening.cmake"
        )
        set_tests_properties(
            dc_rom_upper_wall_pocket_hand_opening
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_upper_wall_pocket_hand_guard
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/upper-wall-pocket-hand-guard-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_upper_wall_pocket_hand_guard.cmake"
        )
        set_tests_properties(
            dc_rom_upper_wall_pocket_hand_guard
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_upper_wall_pocket_hand_close
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/upper-wall-pocket-hand-close-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_upper_wall_pocket_hand_close.cmake"
        )
        set_tests_properties(
            dc_rom_upper_wall_pocket_hand_close
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_four_star_wall_pocket_collision
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/four-star-wall-pocket-collision-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_four_star_wall_pocket_collision.cmake"
        )
        set_tests_properties(
            dc_rom_four_star_wall_pocket_collision
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_four_star_wall_pocket_matching
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/four-star-wall-pocket-matching-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_four_star_wall_pocket_matching.cmake"
        )
        set_tests_properties(
            dc_rom_four_star_wall_pocket_matching
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_upper_wall_pocket_hand_opening_followup
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/upper-wall-pocket-hand-opening-followup-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_upper_wall_pocket_hand_opening_followup.cmake"
        )
        set_tests_properties(
            dc_rom_upper_wall_pocket_hand_opening_followup
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_upper_wall_pocket_hand_opening_expiry
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/upper-wall-pocket-hand-opening-expiry-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_upper_wall_pocket_hand_opening_expiry.cmake"
        )
        set_tests_properties(
            dc_rom_upper_wall_pocket_hand_opening_expiry
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
            NAME dc_rom_side_star_bonus_entrance_animation_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/side-star-bonus-entrance-animation-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_side_star_bonus_entrance_animation_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_side_star_bonus_entrance_animation_semantic
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_side_worm_active_timer_initializer_semantic
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/side-worm-active-timer-initializer-semantic-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_side_worm_active_timer_initializer_semantic.cmake"
        )
        set_tests_properties(
            dc_rom_side_worm_active_timer_initializer_semantic
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
            NAME dc_rom_tower_bonus_entrance
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-bonus-entrance-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_bonus_entrance.cmake"
        )
        set_tests_properties(
            dc_rom_tower_bonus_entrance
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_tower_bonus_stage_selection
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tower-bonus-stage-selection-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_tower_bonus_stage_selection.cmake"
        )
        set_tests_properties(
            dc_rom_tower_bonus_stage_selection
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door_hit_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-hit-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door_hit_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door_hit_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door_completion_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-completion-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door_completion_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door_completion_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door_completion_score_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-completion-score-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door_completion_score_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door_completion_score_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door_completion_scheduler_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-completion-scheduler-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door_completion_scheduler_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door_completion_scheduler_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door_completion_request_prefix
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-completion-request-prefix-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door_completion_request_prefix.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door_completion_request_prefix
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door_completion_wait_gate
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-completion-wait-gate-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door_completion_wait_gate.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door_completion_wait_gate
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door_completion_channel_advance
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-completion-channel-advance-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door_completion_channel_advance.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door_completion_channel_advance
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_giant_door_completion_terminal_channel
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/giant-door-completion-terminal-channel-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_giant_door_completion_terminal_channel.cmake"
        )
        set_tests_properties(
            dc_rom_giant_door_completion_terminal_channel
            PROPERTIES LABELS rom_required
        )

        add_test(
            NAME dc_rom_session_zero_ball_prefix
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-zero-ball-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_zero_ball_prefix.cmake"
        )
        set_tests_properties(dc_rom_session_zero_ball_prefix PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_session_zero_ball_scheduler
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-zero-ball-scheduler-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_zero_ball_scheduler.cmake"
        )
        set_tests_properties(dc_rom_session_zero_ball_scheduler PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_session_zero_ball_countdown
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-zero-ball-countdown-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_zero_ball_countdown.cmake"
        )
        set_tests_properties(dc_rom_session_zero_ball_countdown PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_session_zero_ball_expiry_scheduler
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-zero-ball-expiry-scheduler-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_zero_ball_expiry_scheduler.cmake"
        )
        set_tests_properties(dc_rom_session_zero_ball_expiry_scheduler PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_session_zero_ball_presentation_gate
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-zero-ball-presentation-gate-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_zero_ball_presentation_gate.cmake"
        )
        set_tests_properties(dc_rom_session_zero_ball_presentation_gate PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_session_zero_ball_state_gate
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-zero-ball-state-gate-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_zero_ball_state_gate.cmake"
        )
        set_tests_properties(dc_rom_session_zero_ball_state_gate PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_session_alternate_latch_prefix
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-alternate-latch-prefix-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_alternate_latch_prefix.cmake"
        )
        set_tests_properties(dc_rom_session_alternate_latch_prefix PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_session_alternate_scheduler
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-alternate-scheduler-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_alternate_scheduler.cmake"
        )
        set_tests_properties(dc_rom_session_alternate_scheduler PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_session_alternate_outer
            CONFIGURATIONS Release
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>" "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/session-alternate-outer-v1.csv"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/verify_session_alternate_outer.cmake"
        )
        set_tests_properties(dc_rom_session_alternate_outer PROPERTIES LABELS rom_required)

        add_test(
            NAME dc_rom_walled_star_post_destruction_idle
            CONFIGURATIONS Release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DAPP=$<TARGET_FILE:devils_crush>"
                "-DCONFIG=$<CONFIG>"
                "-DROM=${DC_ROM_PATH}"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/walled-star-post-destruction-idle-v1.csv"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/verify_walled_star_post_destruction_idle.cmake"
        )
        set_tests_properties(
            dc_rom_walled_star_post_destruction_idle
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
