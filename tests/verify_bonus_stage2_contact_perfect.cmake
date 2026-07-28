# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "BS-2 contact-perfect coverage is Release-only, not ${CONFIG}."
    )
endif()

set(
    expected_census_sha256
    "51a3ceb1ef8ad214fe59f39f3a109b314659f2f3dd00afaaf2316e37fb80fc7c"
)
set(
    expected_entry_sha256
    "a394ea8cc7a10db4b24b45ea357f53d5490d74138ca1106249e3941c94c523a7"
)
set(
    expected_state_sha256
    "d28427026d37f15fe8aa39f37906a980091c6cadd1be54e0014d9122c533d0be"
)

set(
    board_positions
    "60:3C" "70:3C" "80:3C" "90:3C"
    "A0:3C" "B0:3C" "C0:3C" "D0:3C"
    "60:4C" "D0:4C"
    "60:5C" "70:5C" "80:5C" "90:5C"
    "A0:5C" "B0:5C" "C0:5C" "D0:5C"
    "60:6C" "D0:6C"
    "60:7C" "70:7C" "80:7C" "90:7C"
    "A0:7C" "B0:7C" "C0:7C" "D0:7C"
)

set(
    recorder_command
    "${APP}"
    --record-state-census "${OUTPUT}"
    --work-ram-patch "1200:2055:02"
    --work-ram-patch "1200:2053:10"
)
set(frame 1304)
foreach(position IN LISTS board_positions)
    string(REPLACE ":" ";" coordinates "${position}")
    list(GET coordinates 0 x_position)
    list(GET coordinates 1 y_position)
    list(
        APPEND recorder_command
        --work-ram-patch "${frame}:3053:C2"
        --work-ram-patch "${frame}:31D3:${x_position}"
        --work-ram-patch "${frame}:3353:${y_position}"
        --work-ram-patch "${frame}:34D3:00"
        --work-ram-patch "${frame}:3F53:99"
        --work-ram-patch "${frame}:3FD3:50"
        --work-ram-patch "${frame}:2D26:01"
    )
    math(EXPR frame "${frame} + 1")
endforeach()

set(entry_output "${OUTPUT}.entries.csv")
set(state_output "${OUTPUT}.state.csv")
list(
    APPEND recorder_command
    --observe-hook "04:4E8F"
    --observe-hook "04:4E97"
    --observe-hook "04:5099"
    --observe-hook "04:5260"
    --observe-hook "04:52ED"
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
        "BS-2 contact-perfect recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 198"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 30"
    "Observe hook 3 entries: 28"
    "Observe hook 4 entries: 28"
    "Observe hook 5 entries: 112"
    "Observe hook 6 entries: 1"
    "Observe hook 7 entries: 87"
    "Observe hook 8 entries: 1"
    "Observe hook 9 entries: 1"
    "Routine trace complete: 289 entries"
    "Work-RAM write trace complete: 3463 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-2 contact-perfect output omitted '${expected_line}'.\n"
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
            "BS-2 contact-perfect ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage2ContactPerfectSemantics.ps1"
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
        "BS-2 contact-perfect analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "BS-2 contact-driven perfect outcome verified."
    "Board completion: 28 post-collision target continuations"
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560"
    "Awards: 12,600 earned units transfer first"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-2 contact-perfect semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()

message(
    STATUS
    "BS-2 contact-perfect artifacts verified.\n${semantics_output}"
)
