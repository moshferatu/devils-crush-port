# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Giant-Skeleton Eyeball steady-idle regression is Release-only.")
endif()

set(expected_census_sha256 "41c10591867cf5bce6440f563e014cd40a0291757230eb77045f7fcd4d51a8bf")
set(expected_trace_sha256 "b2aeb1b68887b3173e728e5885c088367313746ed638ad05245f821082e01ab3")
set(expected_call_sha256 "c0345b3e5fb1a1b979d7a87111b238d99746bddd0d9acf1fffd71a1c45b68b3f")
set(expected_write_sha256 "134f7f71e5d9ebc331e27982b86ff961267ae4e7f267d4f89fc23c3ae48c667f")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:2D20:01"
        --observe-hook "02:4DD9" --observe-hook "02:4DF7" --observe-hook "02:4E24"
        --replace-hook "02:4E24@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "02:4E24@2" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Giant-Skeleton Eyeball steady-idle recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 1"
    "Native replacement 02:4E24@all entries: 1377 of 1377" "Observe hook 1 entries: 320"
    "Observe hook 2 entries: 9" "Observe hook 3 entries: 1377"
    "Routine trace complete: 1706 entries"
    "Routine call trace complete: 1706 calls, 439 writes"
    "Routine replay 02:4E24@2 verified: frame 1211, 51 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Giant-Skeleton Eyeball steady-idle output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Giant-Skeleton Eyeball steady-idle ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 4E24 covers 1,066 complete Eyeball idle walks and 311 live load fallbacks.")
