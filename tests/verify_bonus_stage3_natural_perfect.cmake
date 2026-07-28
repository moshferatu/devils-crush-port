# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "BS-3 natural-perfect coverage is Release-only, not ${CONFIG}."
    )
endif()

set(
    expected_census_sha256
    "dcba750ab8301e926c515a30c5678d014ea5ef8fc977da3d4f62384ff504c439"
)
set(
    expected_entry_sha256
    "50de9c48aaa15a26c4484ba22eca9fd7f8b5a30f97909356343f157a93e96809"
)
set(
    expected_state_sha256
    "89cba55bffa518834204b4ebdc2cb0afde1424c341e2ae3d3532ca6d1d9f7afc"
)

set(entry_output "${OUTPUT}.entries.csv")
set(state_output "${OUTPUT}.state.csv")
execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:2055:03"
        --work-ram-patch "1200:2053:10"
        --work-ram-patch "1303:3851:90"
        --work-ram-patch "1303:3C51:01"
        --work-ram-patch "1303:3852:90"
        --work-ram-patch "1303:3C52:01"
        --work-ram-patch "1303:3853:90"
        --work-ram-patch "1303:3C53:01"
        --observe-hook "04:53AC"
        --observe-hook "04:55DF"
        --observe-hook "04:53BA"
        --observe-hook "04:54E7"
        --observe-hook "04:5626"
        --observe-hook "04:566C"
        --observe-hook "04:56A6"
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
        "BS-3 natural-perfect recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 8"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 3"
    "Observe hook 3 entries: 152"
    "Observe hook 4 entries: 215"
    "Observe hook 5 entries: 3"
    "Observe hook 6 entries: 3"
    "Observe hook 7 entries: 3"
    "Observe hook 8 entries: 1"
    "Observe hook 9 entries: 87"
    "Observe hook 10 entries: 1"
    "Observe hook 11 entries: 1"
    "Routine trace complete: 470 entries"
    "Work-RAM write trace complete: 2421 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-3 natural-perfect output omitted '${expected_line}'.\n"
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
            "BS-3 natural-perfect ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage3NaturalPerfectSemantics.ps1"
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
        "BS-3 natural-perfect analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "BS-3 natural perfect outcome verified."
    "Natural completion: three terminal contacts and indexed finalizers"
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560"
    "Awards: 3,400 earned units include one natural emitted-target contact"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-3 natural-perfect semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()

message(
    STATUS
    "BS-3 natural-perfect artifacts verified.\n${semantics_output}"
)
