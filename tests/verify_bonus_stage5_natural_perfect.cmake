# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "BS-5 natural-perfect coverage is Release-only, not ${CONFIG}."
    )
endif()

set(
    expected_census_sha256
    "5232409f1434cd2f8223eae6ea7303b02325e54205e6f895f3ab1f21894e8033"
)
set(
    expected_entry_sha256
    "44ebaaa83fff908b31bca4055688f7aa322eb642f6e2cc7db15adba14f2ca028"
)
set(
    expected_state_sha256
    "9ffa6901309a6fed1deac690038f1cef1eda42d2cd4906bf9b60b863f3d7d2aa"
)

set(
    recorder_command
    "${APP}"
    --record-state-census "${OUTPUT}"
    --work-ram-patch "1200:2055:05"
    --work-ram-patch "1200:2053:10"
)
foreach(slot IN ITEMS 51 52 53 54 55 56 57 58 59 5A 62 63 64 65 66)
    math(
        EXPR collision_address
        "0x3800 + 0x${slot}"
        OUTPUT_FORMAT HEXADECIMAL
    )
    math(
        EXPR visible_state_address
        "0x3980 + 0x${slot}"
        OUTPUT_FORMAT HEXADECIMAL
    )
    list(
        APPEND recorder_command
        --work-ram-patch "1303:${collision_address}:90"
        --work-ram-patch "1303:${visible_state_address}:02"
    )
endforeach()

set(entry_output "${OUTPUT}.entries.csv")
set(state_output "${OUTPUT}.state.csv")
list(
    APPEND recorder_command
    --observe-hook "05:6E0E"
    --observe-hook "05:7100"
    --observe-hook "05:6E16"
    --observe-hook "05:7112"
    --observe-hook "04:44DB"
    --observe-hook "04:4555"
    --observe-hook "04:456C"
    --observe-hook "04:464F"
    --routine-trace "${entry_output}"
    --work-ram-write-range "2D00-2DFF"
    --work-ram-write-trace "${state_output}"
    --rom "${ROM}"
)
execute_process(
    COMMAND ${recorder_command}
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-5 natural-perfect recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 32"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 15"
    "Observe hook 3 entries: 3"
    "Observe hook 4 entries: 30"
    "Observe hook 5 entries: 1"
    "Observe hook 6 entries: 87"
    "Observe hook 7 entries: 1"
    "Observe hook 8 entries: 1"
    "Routine trace complete: 139 entries"
    "Work-RAM write trace complete: 2722 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-5 natural-perfect output omitted '${expected_line}'.\n"
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
            "BS-5 natural-perfect ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage5NaturalPerfectSemantics.ps1"
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
        "BS-5 natural-perfect analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "BS-5 natural perfect outcome verified."
    "Natural completion: 15 terminal contacts reduce 2D2E from 15 to 0"
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560"
    "Awards: 7,500 earned units transfer first"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-5 natural-perfect semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()

message(
    STATUS
    "BS-5 natural-perfect artifacts verified.\n${semantics_output}"
)
