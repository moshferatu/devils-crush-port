# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Release")
    set(expected_census_sha256 "c12f290cd61c254e573b362ab572cb1afb19a5ddaabc27ca77bcdd19e51c71d8")
elseif(CONFIG STREQUAL "Debug")
    set(expected_census_sha256 "5ffb267191ede4e70121638ae0198463a25e95b71582d82bb6e84457bb80248e")
else()
    message(FATAL_ERROR "Unsupported upper-hand guard configuration: ${CONFIG}")
endif()
set(expected_trace_sha256 "2acc56ad951cd8774a22d088209b33482b3f2e0b7659de5bdcb8439a76afb7b4")
set(expected_call_sha256 "1e03037d506dcc057d851a85e24783a1af367ea407a920a824d6d61bd042c473")
set(expected_write_sha256 "edb384280b11fa4cab1e6527ac1ee928d13c9dd7b2753af938d76715eef38a83")
set(expected_state_sha256 "eccef7f1968b9576f34ba291712d1e67d52cbf428c1c8c74cc12855c6d5c1df0")

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
        --work-ram-patch "1222:2D33:01"
        --observe-hook "02:59A5"
        --replace-hook "02:59A5@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2D33-2D33"
        --work-ram-write-range "3040-3040"
        --work-ram-write-range "30C0-30C0"
        --work-ram-write-range "3840-3840"
        --work-ram-write-range "3940-3940"
        --work-ram-write-range "39C0-39C0"
        --work-ram-write-range "3A40-3A40"
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
    message(FATAL_ERROR "Upper Wall-Pocket hand guard recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 5"
    "Native replacement 02:59A5@all entries: 298 of 298"
    "Observe hook 1 entries: 298"
    "Routine trace complete: 298 entries"
    "Routine call trace complete: 298 calls, 26 writes"
    "Work-RAM write trace complete: 539 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Upper-hand guard output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Upper-hand guard ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Upper-hand guard artifacts verified: ${actual_census_sha256}, ${actual_trace_sha256}, ${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}")
