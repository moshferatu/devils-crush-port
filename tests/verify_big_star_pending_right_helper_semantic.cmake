# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Big-Star pending-right helper regression is Release-only.")
endif()

set(expected_census_sha256 "44dd43af3a76c12707bb308896041f9e851ce696bd7abb9e9961912e2c8d19fe")
set(expected_trace_sha256 "fa2f8302f27bdbd530922800cbd251de52214897d570764b618a37639bb3cd43")
set(expected_call_sha256 "92db5ebb0d399248580c27a68c5f98919954c8c8cac20c4a57a9750317ee1686")
set(expected_write_sha256 "2bf7d841881f963d809fb98cd10bf20431af108f70764fb5c656082f714543b3")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334B:2C"
        --work-ram-patch "1200:33CB:01"
        --work-ram-patch "1201:2D04:02"
        --observe-hook "02:570A" --observe-hook "02:5738"
        --replace-hook "02:5738@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "02:5738@1" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Big-Star pending-right helper recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 3"
    "Native replacement 02:5738@all entries: 1 of 1"
    "Observe hook 1 entries: 490" "Observe hook 2 entries: 1"
    "Routine trace complete: 491 entries"
    "Routine call trace complete: 491 calls, 580 writes"
    "Routine replay 02:5738@1 verified: frame 1221, 228 body cycles, 6 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Big-Star pending-right helper output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Big-Star pending-right helper ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 5738 covers the observed fixed Big-Star pending-right phase-three helper.")
