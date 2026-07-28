# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "BS-6 natural-perfect coverage is Release-only, not ${CONFIG}."
    )
endif()

set(
    expected_census_sha256
    "fef4dc91446e524de66892be7692b88d7782cb0113cde447c569a3c8a7b1c08a"
)
set(
    expected_entry_sha256
    "7e05d90d709b89945587846e1b4b1080dc5ab9d9212186c2af8dc56910b0f2e1"
)
set(
    expected_state_sha256
    "d1dd5a31f20306e8aa776b599707d3c9bcbf65137730ffe2270a35c925a4ad4b"
)

set(entry_output "${OUTPUT}.entries.csv")
set(state_output "${OUTPUT}.state.csv")
execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:2055:06"
        --work-ram-patch "1200:2053:10"
        --work-ram-patch "1303:3834:90"
        --work-ram-patch "1303:3C34:01"
        --work-ram-patch "1303:3835:90"
        --work-ram-patch "1303:3C35:01"
        --observe-hook "05:71E3"
        --observe-hook "05:71F8"
        --observe-hook "05:71EB"
        --observe-hook "05:72A8"
        --observe-hook "04:44DB"
        --observe-hook "04:4555"
        --observe-hook "04:456C"
        --observe-hook "04:464F"
        --routine-trace "${entry_output}"
        --work-ram-write-range "2D00-2DFF"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-6 natural-perfect recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 6"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 2"
    "Observe hook 3 entries: 3"
    "Observe hook 4 entries: 2"
    "Observe hook 5 entries: 1"
    "Observe hook 6 entries: 87"
    "Observe hook 7 entries: 1"
    "Observe hook 8 entries: 1"
    "Routine trace complete: 98 entries"
    "Work-RAM write trace complete: 2307 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-6 natural-perfect output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census entry state)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "entry")
        set(artifact_path "${entry_output}")
    else()
        set(artifact_path "${state_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "BS-6 natural-perfect ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage6NaturalPerfectSemantics.ps1"
        -RomPath "${ROM}"
        -EntryPath "${entry_output}"
        -StatePath "${state_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-6 natural-perfect analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "BS-6 natural perfect outcome verified."
    "Natural completion: paired terminal contacts reduce 2D2E from 2 to 0"
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560"
    "Awards: 4,100 earned units transfer first"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-6 natural-perfect semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()

message(
    STATUS
    "BS-6 natural-perfect artifacts verified.\n${semantics_output}"
)
