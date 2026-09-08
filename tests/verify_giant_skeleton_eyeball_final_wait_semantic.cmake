# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Giant-Skeleton Eyeball final-wait regression is Release-only.")
endif()

set(expected_census_sha256 "41c10591867cf5bce6440f563e014cd40a0291757230eb77045f7fcd4d51a8bf")
set(expected_trace_sha256 "9421093ceeb01325fc608876b142aba030999ab1707600a31d406851ed7a3362")
set(expected_call_sha256 "4a04c0c36c1e43939e2e0f96b8366311c1a7320acf89f392a343c0bc0a4ec69e")
set(expected_write_sha256 "bb359392ced34c7ffce95238b069b30d480dee19594b4ca39e395649081c4264")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:2D20:01"
        --observe-hook "02:4DD9" --observe-hook "02:4DF7"
        --observe-hook "02:4E24" --observe-hook "02:4F3D"
        --replace-hook "02:4F3D@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "02:4F3D@2" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Giant-Skeleton Eyeball final-wait recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 1"
    "Native replacement 02:4F3D@all entries: 224 of 224" "Observe hook 1 entries: 320"
    "Observe hook 2 entries: 9" "Observe hook 3 entries: 1377" "Observe hook 4 entries: 224"
    "Routine trace complete: 1930 entries"
    "Routine call trace complete: 1930 calls, 1291 writes"
    "Routine replay 02:4F3D@2 verified: frame 3257, 102 body cycles, 2 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Giant-Skeleton Eyeball final-wait output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Giant-Skeleton Eyeball final-wait ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 4F3D covers all 224 final-animation calls; legacy artifact hashes preserved.")
