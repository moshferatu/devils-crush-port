# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Big-Star active-controller regression is Release-only.")
endif()

set(expected_census_sha256 "6110f853aa00423d97598e10ec1a64abe507bb6b8629e6f3768a3f40f0c3c78d")
set(expected_call_sha256 "a898989bf2841487729b3e5491755c1b7035502bce8c407fcff4f509f53ac4e5")
set(expected_write_sha256 "2b63f8c4f45981a226a38f364b286160f6a857a556e9cca895a164eed18447ba")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334B:2C"
        --work-ram-patch "1200:33CB:01"
        --observe-hook "02:5674"
        --observe-hook "02:570A"
        --observe-hook "02:5722"
        --replace-hook "02:5674@all"
        --replace-hook "02:570A@all"
        --replace-hook "02:5722@all"
        --routine-call-trace "${call_output}"
        --routine-replay "02:5674@4"
        --routine-replay "02:5722@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Big-Star pending-gate recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 2"
    "Native replacement 02:5674@all entries: 418 of 490"
    "Native replacement 02:570A@all entries: 280 of 291"
    "Native replacement 02:5722@all entries: 1 of 1"
    "Observe hook 1 entries: 490"
    "Observe hook 2 entries: 291"
    "Observe hook 3 entries: 1"
    "Routine call trace complete: 782 calls, 22845 writes"
    "Routine replay 02:5674@4 verified: frame 1224, 222 body cycles, 5 ordered writes"
    "Routine replay 02:5722@1 verified: frame 1221, 240 body cycles, 6 ordered writes"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Big-Star pending-gate output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Semantic Big-Star pending-gate ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 5674 preserves 199 full rotation walks and adds 219 independent JSR fallbacks; 570A admits 280 of 291 child entries and its observed pending-left child 5722 is native.")
