# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Giant-Skeleton Eyeball opening-wait regression is Release-only.")
endif()

set(expected_census_sha256 "41c10591867cf5bce6440f563e014cd40a0291757230eb77045f7fcd4d51a8bf")
set(expected_trace_sha256 "ed3ed66f354edd1c4494659c31bdbd5245d80375a63ca0ecc50acb261a8c42e8")
set(expected_call_sha256 "8d2e0f115a332c83d77c3968dee899ab3075dd8ebddad8c27f9c4164b96c4342")
set(expected_write_sha256 "4dbf1b5f665ba86a1b1d22094a532a4ba9df63c731e315d2d30eb9aa650b7e85")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:2D20:01"
        --observe-hook "02:4DD9" --observe-hook "02:4DF7"
        --replace-hook "02:4DF7@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "02:4DF7@2" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Giant-Skeleton Eyeball opening-wait recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 1"
    "Native replacement 02:4DF7@all entries: 7 of 9" "Observe hook 1 entries: 320"
    "Observe hook 2 entries: 9" "Routine trace complete: 329 entries"
    "Routine call trace complete: 329 calls, 163 writes"
    "Routine replay 02:4DF7@2 verified: frame 1202, 33 body cycles, 1 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Giant-Skeleton Eyeball opening-wait output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Giant-Skeleton Eyeball opening-wait ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 4DF7 covers all seven observed Giant-Skeleton Eyeball opening timer waits.")
