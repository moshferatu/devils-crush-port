add_executable(
    dc_core_tests
    core_tests.cpp
)

target_link_libraries(
    dc_core_tests
    PRIVATE
        DevilsCrush::Core
        DevilsCrush::Emulation
)

target_compile_features(dc_core_tests PRIVATE cxx_std_20)
dc_enable_strict_warnings(dc_core_tests)

add_test(NAME dc_core_tests COMMAND dc_core_tests)

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
    endif()
endif()
