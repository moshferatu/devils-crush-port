# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "BS-1 natural-perfect coverage is Release-only, not ${CONFIG}."
    )
endif()

set(
    expected_census_sha256
    "a64ab169a786eb3df1aa6ea4622a92e9af1fd6ccc9bfbb9e81d3177e3bfe0a3c"
)
set(
    expected_entry_sha256
    "a66366020d8a9ffce74f9ccd608d6f173aad85adda82ee6f8d9b242ece57f016"
)
set(
    expected_state_sha256
    "41901f52d3e610a775263296193c9819f4b753bc02b0e18a07ff200ed01eccad"
)

set(entry_output "${OUTPUT}.entries.csv")
set(state_output "${OUTPUT}.state.csv")
execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:2055:01"
        --work-ram-patch "1200:2053:10"
        --work-ram-patch "1303:3834:90"
        --work-ram-patch "1303:3C34:01"
        --work-ram-patch "1303:3835:90"
        --work-ram-patch "1303:3C35:01"
        --work-ram-patch "1303:3850:90"
        --work-ram-patch "1303:3C50:01"
        --work-ram-patch "1303:3851:90"
        --work-ram-patch "1303:3C51:01"
        --work-ram-patch "1303:3852:90"
        --work-ram-patch "1303:3C52:01"
        --observe-hook "04:44C6"
        --observe-hook "04:44CE"
        --observe-hook "04:47B5"
        --observe-hook "04:47D5"
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
        "BS-1 natural-perfect recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 12"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 9"
    "Observe hook 3 entries: 5"
    "Observe hook 4 entries: 5"
    "Observe hook 5 entries: 1"
    "Observe hook 6 entries: 87"
    "Observe hook 7 entries: 1"
    "Observe hook 8 entries: 1"
    "Routine trace complete: 110 entries"
    "Work-RAM write trace complete: 2202 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-1 natural-perfect output omitted '${expected_line}'.\n"
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
            "BS-1 natural-perfect ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage1NaturalPerfectSemantics.ps1"
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
        "BS-1 natural-perfect analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "BS-1 natural perfect outcome verified."
    "Natural completion: five terminal target contacts"
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560"
    "Awards: 1,000 earned units transfer first"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-1 natural-perfect semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()

message(
    STATUS
    "BS-1 natural-perfect artifacts verified.\n${semantics_output}"
)
