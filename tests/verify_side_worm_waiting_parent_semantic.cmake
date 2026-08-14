# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Side-Worm waiting-parent regression is Release-only.")
endif()
set(expected_census_sha256 "297d2de96400e7e6e660a05fdb86a5dacf18a60d99a2c69b8b9423f3b777339d")
set(expected_trace_sha256 "39bdbe8fa172a22447c592b0a2a044092ebf882a531ca43ab697cd6f99dc10c6")
set(expected_call_sha256 "5a79773c745026415adf4162692978e6eb52222653faa730225779d95de148dc")
set(expected_write_sha256 "2840f38c5a5ce5a9777105a62a23f0d424c5ba0cdb7b608a2162e397708074e2")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334E:2C" --work-ram-patch "1200:33CE:01" --work-ram-patch "1200:37CE:06"
        --work-ram-patch "1200:334F:2C" --work-ram-patch "1200:33CF:01" --work-ram-patch "1200:37CF:02"
        --observe-hook "02:5CC5" --replace-hook "02:5CC5@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "02:5CC5@1" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Side-Worm waiting-parent recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 6"
    "Native replacement 02:5CC5@all entries: 1760 of 1760" "Observe hook 1 entries: 1760"
    "Routine trace complete: 1760 entries" "Routine call trace complete: 1760 calls, 452 writes"
    "Routine replay 02:5CC5@1 verified: frame 1221, 72 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Side-Worm waiting-parent output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Side-Worm waiting-parent ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()
message(STATUS "Semantic 5CC5 covers all 1,760 stopped Side-Worm parent returns, including interrupt passages.")
