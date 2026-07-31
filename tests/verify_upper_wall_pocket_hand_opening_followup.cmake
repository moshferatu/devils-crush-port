# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(expected_census_sha256 "6b598922dcb8e8e668c537f7461504108cf5840221cc751743021d96175f008b")
elseif(CONFIG STREQUAL "Release")
    set(expected_census_sha256 "9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed")
else()
    message(FATAL_ERROR "Unsupported upper-hand-opening-followup configuration: ${CONFIG}")
endif()

set(expected_trace_sha256 "b7d9ad263c05ebcbe441dd829947f36a30571ad693a4dc5e73798b62fc27ca05")
set(expected_call_sha256 "8096c9e92792beabce21640fce8a0eeb516577c91ce1b185e515723cbb08ad26")
set(expected_write_sha256 "f9b54e63be8bda6487d9499589586156d0c7bc5570dbdb3cb204f2ee2bd0c1f4")
set(expected_state_sha256 "638d7d5d24d36dba5188eaefa8ae48af1b3af8275fb8b93abe1f63792f1fbbab")

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
        --observe-hook "02:59C7"
        --replace-hook "02:5991@all"
        --replace-hook "02:59A5@all"
        --replace-hook "02:59C7@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "3040-3040"
        --work-ram-write-range "30C0-30C0"
        --work-ram-write-range "3A40-3A40"
        --work-ram-write-range "3AC0-3AC0"
        --work-ram-write-range "3B40-3B40"
        --work-ram-write-range "3BC0-3BC0"
        --work-ram-write-range "3F40-3F40"
        --work-ram-write-range "3FC0-3FC0"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Upper Wall-Pocket hand opening-followup recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 4"
    "Native replacement 02:5991@all entries: 174 of 174"
    "Native replacement 02:59A5@all entries: 1 of 1"
    "Native replacement 02:59C7@all entries: 1 of 1"
    "Observe hook 1 entries: 1"
    "Routine trace complete: 1 entries"
    "Routine call trace complete: 1 calls, 9 writes"
    "Work-RAM write trace complete: 4436 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Upper-hand opening-followup output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Upper-hand opening-followup ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Upper-hand opening-followup artifacts verified: ${actual_census_sha256}, ${actual_trace_sha256}, ${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}")
