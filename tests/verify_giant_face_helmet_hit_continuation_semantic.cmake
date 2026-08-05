# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Giant-Face helmet continuation configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "477150df2372bb428f022c26817c3476788a46e9e3f7c8d1b5f76949d56624e0")
set(expected_trace_sha256 "d025fbad7bb790597ff84fdc74e1e6a121f48751993847be64f56aa80b25688c")
set(expected_call_sha256 "7c05181b1bbc46cf83645918e7844ddfd3f5d8473aee2793014877774a01e4d7")
set(expected_write_sha256 "8c8f6dd4d67cf6feac317c921dbea426117f74740eb7bccc7fb4200affefe1d9")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:382A:80"
        --observe-hook "02:4C91"
        --observe-hook "02:4CFB"
        --replace-hook "02:4CFB@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "02:4CFB@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Giant-Face helmet continuation recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 1"
    "Native replacement 02:4CFB@all entries: 1 of 1"
    "Observe hook 1 entries: 476"
    "Observe hook 2 entries: 1"
    "Routine trace complete: 477 entries"
    "Routine call trace complete: 477 calls, 1016 writes"
    "Routine replay 02:4CFB@1 verified: frame 1222, 396 body cycles, 15 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Giant-Face helmet continuation output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Giant-Face helmet continuation ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual: ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 4CFB continuation restores the ball and schedules 4D0B exactly.")
