# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Side-Worm active timer initializer regression is Release-only.")
endif()

set(expected_census_sha256 "cc2e26bb3c446937072eb8d963dd702c803c908fe17e9be493c737cc4a375020")
set(expected_trace_sha256 "dff68039276f61b81e4c82ae18359735a9ff3badf9ce9db7ce969105821a548a")
set(expected_call_sha256 "3ea5a02a93d957f89a69eb38c29611d4e71d75d23190bfe931ef594da4c6b221")
set(expected_write_sha256 "f64e6c64b41606dc771a2a0f15cda3b92d398501b5979c9f78267bd8f4750e1e")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334E:2C"
        --work-ram-patch "1200:33CE:01"
        --work-ram-patch "1200:37CE:06"
        --work-ram-patch "1200:2D18:06"
        --observe-hook "02:5CC5" --observe-hook "02:5D4A" --observe-hook "02:5DFC"
        --replace-hook "02:5D4A@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "02:5D4A@1" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Side-Worm active timer initializer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 4"
    "Native replacement 02:5D4A@all entries: 1 of 1"
    "Observe hook 1 entries: 1591" "Observe hook 2 entries: 1" "Observe hook 3 entries: 20"
    "Routine trace complete: 1612 entries"
    "Routine call trace complete: 1612 calls, 667 writes"
    "Routine replay 02:5D4A@1 verified: frame 1997, 192 body cycles, 7 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Side-Worm active timer initializer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Side-Worm active timer initializer ${artifact} changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 5D4A covers the fixed Side-Worm active timer setup before mixed motion.")
