# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(expected_census_sha256 "45a7a4ef3c92924574d97666aa03cfae4f36d837e2a74efc3eec71c6ecd4fba9")
elseif(CONFIG STREQUAL "Release")
    set(expected_census_sha256 "7a3466734aa3856e9158828057ce31f7a9854d95a9ce8e9b6768b372b3bcb9df")
else()
    message(FATAL_ERROR "Unsupported upper-hand-close-loop configuration: ${CONFIG}")
endif()
set(expected_trace_sha256 "2fbfe2597e236fc6f6249f9388f61999514985c88b339cfd9c9a4a4232542096")
set(expected_call_sha256 "7b7516225b1a6fe7c1612c42c386816c69cef5ca04c294b76e720e623796fb4e")
set(expected_write_sha256 "4514ec8143334011d1856f0163c460ce1d454601e2763307e2a1fd08cb8d352b")
set(expected_state_sha256 "d9608965ca1ec3f589fe9e048c9598e8a77a9a18a09d1fdf7a1ea5bd973595f2")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:3340:2C"
        --work-ram-patch "1200:33C0:01"
        --work-ram-patch "1200:3F40:C7"
        --work-ram-patch "1200:3FC0:59"
        --work-ram-patch "1221:3840:80"
        --observe-hook "02:59C7"
        --replace-hook "02:59C7@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "3040-3040"
        --work-ram-write-range "30C0-30C0"
        --work-ram-write-range "3840-3840"
        --work-ram-write-range "3A40-3A40"
        --work-ram-write-range "3B40-3B40"
        --work-ram-write-range "3BC0-3BC0"
        --work-ram-write-range "3DC0-3DC0"
        --work-ram-write-range "3F40-3F40"
        --work-ram-write-range "3FC0-3FC0"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Upper-hand close-loop recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 5"
    "Native replacement 02:59C7@all entries: 3 of 3"
    "Observe hook 1 entries: 3"
    "Routine trace complete: 3 entries"
    "Routine call trace complete: 3 calls, 45 writes"
    "Work-RAM write trace complete: 5226 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Upper-hand close-loop output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write state)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${state_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set("actual_${artifact}_sha256" "${actual_sha256}")
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Upper-hand close-loop ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Upper-hand close-loop artifacts verified: ${actual_census_sha256}, ${actual_trace_sha256}, ${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}")
