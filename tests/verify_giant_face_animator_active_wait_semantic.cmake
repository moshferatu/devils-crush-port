# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Giant-Face active-animator wait regression is Release-only.")
endif()

set(expected_census_sha256 "a99ac333c3f0dbbde8e2df778e5921482c34b33eb4d790e206fa8d72702ac5c3")
set(expected_trace_sha256 "91f3d482434689d72f4c3595d42d9588d5d502137b8d4f56a2dbe094feb35b60")
set(expected_call_sha256 "2a0b4e2eb710ca66ae90f96143230a0055bcd1041e971486e4b15f48f08f6d38")
set(expected_write_sha256 "156a20e944e971c7d58d1f2e4e85286a6e39c98a277121a7edbf70728cc62aa3")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:2D0E:01"
        --observe-hook "03:767E" --observe-hook "03:76C5"
        --replace-hook "03:76C5@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "03:76C5@2" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Giant-Face active-animator wait recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 1"
    "Native replacement 03:76C5@all entries: 639 of 872"
    "Observe hook 1 entries: 1" "Observe hook 2 entries: 872"
    "Routine trace complete: 873 entries"
    "Routine call trace complete: 873 calls, 9587 writes"
    "Routine replay 03:76C5@2 verified: frame 1224, 27 body cycles, 1 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Giant-Face active-animator wait output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
    if(NOT actual_sha256 STREQUAL "${expected_${artifact}_sha256}")
        message(FATAL_ERROR "Semantic Giant-Face active-animator wait ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 76C5 covers 639 horizon-safe active Giant-Face animator waits.")
