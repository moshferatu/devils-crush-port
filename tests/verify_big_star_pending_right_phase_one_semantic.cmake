# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Big-Star pending-right phase-one regression is Release-only.")
endif()

set(expected_census_sha256 "21cf3e22b95eb07f7f10602f29aa879af209405a0582b6ced272dc068b3424cb")
set(expected_trace_sha256 "efcb9a7fee3463b6336904d3ea5612a53aafb28b8eccecc505f9ff3554d5eacf")
set(expected_call_sha256 "8f8ec69a604f87f8cdb81c7e508c539a02e02bae12db67f279e500a0a3004817")
set(expected_write_sha256 "04a64f0bc710c4fe8ad9ea25bd5c123a498a67b3619a9ab97b4b663a968dc5e5")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334B:2C"
        --work-ram-patch "1200:33CB:01"
        --work-ram-patch "1201:2D04:02"
        --work-ram-patch "1201:3B4B:01"
        --observe-hook "02:570A" --observe-hook "02:5738"
        --replace-hook "02:5738@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "02:5738@1" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Big-Star pending-right phase-one recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 4"
    "Native replacement 02:5738@all entries: 1 of 1"
    "Observe hook 1 entries: 374" "Observe hook 2 entries: 1"
    "Routine trace complete: 375 entries"
    "Routine call trace complete: 375 calls, 552 writes"
    "Routine replay 02:5738@1 verified: frame 1221, 228 body cycles, 6 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Big-Star pending-right phase-one output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Big-Star pending-right phase-one ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 5738 covers the verified Big-Star pending-right phase-one helper.")
