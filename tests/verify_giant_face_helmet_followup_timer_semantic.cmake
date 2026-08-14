# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Giant-Face helmet follow-up timer configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "477150df2372bb428f022c26817c3476788a46e9e3f7c8d1b5f76949d56624e0")
set(expected_trace_sha256 "b6093e56a4d30b99d1b9a2bca3e03bb2ca8cb2ddf60f4d470ba9ebead53f0ce2")
set(expected_call_sha256 "dc286a743771066a5f646dae6289599f526d7d6df3f630f2e22b8addf6c2cbf9")
set(expected_write_sha256 "56f59a6e5fd07422461a1fa3b15c474aaea36e2e404fc93092562e442857a7c9")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:382A:80"
        --observe-hook "02:4C91"
        --observe-hook "02:4CFB"
        --observe-hook "02:4D0B"
        --replace-hook "02:4D0B@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "02:4D0B@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Giant-Face helmet follow-up timer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 1"
    "Native replacement 02:4D0B@all entries: 92 of 93"
    "Observe hook 1 entries: 476"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 93"
    "Routine trace complete: 570 entries"
    "Routine call trace complete: 570 calls, 1504 writes"
    "Routine replay 02:4D0B@1 verified: frame 1518, 60 body cycles, 1 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Giant-Face helmet follow-up timer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_${artifact}_sha256)
        message(FATAL_ERROR "Semantic Giant-Face helmet follow-up timer ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual: ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 4D0B covers helmet timer waits and advances while preserving the phase-six fallback.")
