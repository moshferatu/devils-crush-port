# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Release")
    set(expected_census_sha256 "9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed")
elseif(CONFIG STREQUAL "Debug")
    set(expected_census_sha256 "6b598922dcb8e8e668c537f7461504108cf5840221cc751743021d96175f008b")
else()
    message(FATAL_ERROR "Unsupported upper-hand opening-expiry configuration: ${CONFIG}")
endif()
set(expected_trace_sha256 "7911defdc4bfe101ca7bc0df0c4133204ecb411837b20ee72f6e0a0d40ec63c1")
set(expected_call_sha256 "895f65d70b47942f5fbd6ae691b66505749283ba2823b100dc5c6c4d73b6a7ae")
set(expected_write_sha256 "fa415070ae64c33cf45ecb5dd700a2f3e2ae873658c1f2569eb34ef7518b3232")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:3340:2C"
        --work-ram-patch "1200:33C0:01"
        --work-ram-patch "1222:3B40:01"
        --work-ram-patch "1222:3BC0:00"
        --observe-hook "02:59E0"
        --observe-hook "02:5A08"
        --observe-hook "02:5A0B"
        --observe-hook "02:5A1B"
        --replace-hook "02:5991@all"
        --replace-hook "02:59A5@all"
        --replace-hook "02:59C7@all"
        --replace-hook "02:59E0@all"
        --replace-hook "02:5A08@all"
        --replace-hook "02:5A0B@all"
        --replace-hook "02:5A1B@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "3040-3040"
        --work-ram-write-range "30C0-30C0"
        --work-ram-write-range "39C0-39C0"
        --work-ram-write-range "3A40-3A40"
        --work-ram-write-range "3AC0-3AC0"
        --work-ram-write-range "3B40-3B40"
        --work-ram-write-range "3BC0-3BC0"
        --work-ram-write-range "3D40-3D40"
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
    message(FATAL_ERROR "Upper Wall-Pocket hand opening-expiry recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 4"
    "Native replacement 02:5991@all entries: 174 of 174"
    "Native replacement 02:59A5@all entries: 1 of 1"
    "Native replacement 02:59C7@all entries: 1 of 1"
    "Native replacement 02:59E0@all entries: 5 of 5"
    "Native replacement 02:5A08@all entries: 1 of 1"
    "Native replacement 02:5A0B@all entries: 1 of 1"
    "Native replacement 02:5A1B@all entries: 9 of 9"
    "Observe hook 1 entries: 5"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 9"
    "Routine trace complete: 16 entries"
    "Routine call trace complete: 16 calls, 112 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Upper-hand opening-expiry output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Upper-hand opening-expiry ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Upper-hand opening-expiry artifacts verified: ${actual_census_sha256}, ${actual_trace_sha256}, ${actual_call_sha256}, ${actual_write_sha256}")
