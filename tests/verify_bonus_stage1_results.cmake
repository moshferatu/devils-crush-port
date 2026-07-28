# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    # The analyzer guards both result branches statically. Keep the scored
    # route as the cross-configuration dynamic oracle; Release additionally
    # covers the longer perfect-award transfer.
    set(results Scored)
elseif(CONFIG STREQUAL "Release")
    set(results Scored Perfect)
else()
    message(
        FATAL_ERROR
        "Unsupported BS-1-result configuration: ${CONFIG}"
    )
endif()

foreach(result IN LISTS results)
    if(result STREQUAL "Scored")
        set(
            expected_census_sha256
            "f9d1d29b90774a62b495bc57c00fbe6df18c676351441f92893ceced8771979b"
        )
        set(
            expected_state_sha256
            "6f06098ff55ee92e2dac4d4e5b8ebdea34c9fd4cb49f4a0cb8c08fa5629a746c"
        )
        set(patch_count 3)
        set(branch_hook "04:46A4")
        set(glyph_entries 181)
        set(state_writes 1081)
        set(extra_patch)
    else()
        set(
            expected_census_sha256
            "3d0c8e2de33d3fe6c48ea085205fb7e0d362a2d27c8b96ba0f02fabb4f2aaefb"
        )
        set(
            expected_state_sha256
            "f2e47ee8eea9e533d72855e097ae2744b76df3d89f2288f22ad3b59fc21c465c"
        )
        set(patch_count 4)
        set(branch_hook "04:464F")
        set(glyph_entries 874)
        set(state_writes 7191)
        set(
            extra_patch
            --work-ram-patch
            "1459:2DBA:01"
        )
    endif()

    string(TOLOWER "${result}" result_lower)
    set(result_output "${OUTPUT}.${result_lower}.csv")
    set(state_output "${result_output}.state.csv")
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census
            "${result_output}"
            --work-ram-patch
            "1300:2055:01"
            --work-ram-patch
            "1300:2053:10"
            --work-ram-patch
            "1459:2DB7:01"
            ${extra_patch}
            --pass-through-hook
            "04:456C"
            --pass-through-hook
            "04:45F0"
            --pass-through-hook
            "${branch_hook}"
            --pass-through-hook
            "04:4DC4"
            --work-ram-write-range
            "2D00-2DFF"
            --work-ram-write-trace
            "${state_output}"
            --rom
            "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(
            FATAL_ERROR
            "BS-1 ${result} result recorder failed (${recorder_result}).\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Work-RAM patches applied: ${patch_count}"
        "Pass-through hook 1 entries: 1"
        "Pass-through hook 2 entries: 1"
        "Pass-through hook 3 entries: 1"
        "Pass-through hook 4 entries: ${glyph_entries}"
        "Work-RAM write trace complete: ${state_writes} selected writes"
    )
        string(FIND "${recorder_output}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-1 ${result} output omitted '${expected_line}'.\n"
                "${recorder_output}${recorder_error}"
            )
        endif()
    endforeach()

    file(SHA256 "${result_output}" actual_census_sha256)
    if(NOT actual_census_sha256 STREQUAL expected_census_sha256)
        message(
            FATAL_ERROR
            "BS-1 ${result} census changed.\n"
            "Expected: ${expected_census_sha256}\n"
            "Actual:   ${actual_census_sha256}\n"
        )
    endif()
    file(SHA256 "${state_output}" actual_state_sha256)
    if(NOT actual_state_sha256 STREQUAL expected_state_sha256)
        message(
            FATAL_ERROR
            "BS-1 ${result} selected state changed.\n"
            "Expected: ${expected_state_sha256}\n"
            "Actual:   ${actual_state_sha256}\n"
        )
    endif()

    execute_process(
        COMMAND
            powershell
            -NoProfile
            -ExecutionPolicy
            Bypass
            -File
            "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage1ResultSemantics.ps1"
            -Result
            "${result}"
            -RomPath
            "${ROM}"
            -StatePath
            "${state_output}"
        RESULT_VARIABLE semantics_result
        OUTPUT_VARIABLE semantics_output
        ERROR_VARIABLE semantics_error
    )
    if(NOT semantics_result EQUAL 0)
        message(
            FATAL_ERROR
            "BS-1 ${result} analysis failed (${semantics_result}).\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
    foreach(expected_line IN ITEMS
        "BS-1 ${result} result semantics verified."
        "${result} route:"
        "${result} presentation:"
    )
        string(FIND "${semantics_output}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-1 ${result} semantics omitted '${expected_line}'.\n"
                "${semantics_output}${semantics_error}"
            )
        endif()
    endforeach()
    if(result STREQUAL "Perfect")
        string(FIND "${semantics_output}" "Perfect award:" award_offset)
        if(award_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-1 Perfect semantics omitted the award transfer.\n"
                "${semantics_output}${semantics_error}"
            )
        endif()
    endif()
    message(
        STATUS
        "BS-1 ${result} artifacts verified: "
        "${actual_census_sha256}, ${actual_state_sha256}\n"
        "${semantics_output}"
    )
endforeach()
