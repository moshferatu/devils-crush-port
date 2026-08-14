# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported right Giant-Face helmet continuation configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "653c6cc868191f0f2e1c3aa7c01536353dcd48b9555d08a04fb7962590f0f5e9")
set(expected_trace_sha256 "a0a7ed6a108110f757b460a3479c0a3f3009f0767bb9a1495d02c546047c10de")
set(expected_call_sha256 "65875fee04d6dd139f5e3c9612519871b606bc40fa8b317c63c4b67d60a8e28f")
set(expected_write_sha256 "887a94ced66802395af27d63c3cc20685bc8453ca914a2044d89619aaa98d510")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:382B:80"
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
    message(FATAL_ERROR "Semantic right Giant-Face helmet continuation recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 1"
    "Native replacement 02:4CFB@all entries: 1 of 1"
    "Observe hook 1 entries: 357"
    "Observe hook 2 entries: 1"
    "Routine trace complete: 358 entries"
    "Routine call trace complete: 358 calls, 829 writes"
    "Routine replay 02:4CFB@1 verified: frame 1222, 396 body cycles, 15 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic right Giant-Face helmet continuation output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic right Giant-Face helmet continuation ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual: ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 4CFB continuation restores the right ball channel and schedules 4D0B exactly.")
