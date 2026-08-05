# SPDX-License-Identifier: GPL-3.0-or-later

# This focused matching-arrow route extends the collision prefix through its
# zero-rotation selector decision, special-path latch clear, audio-command
# load, post-audio 100-unit score, opening timer, and post-helper appearance/
# scheduler handoff. The shared audio and appearance helpers remain assembly
# backed.
foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(expected_census_sha256 "d1fc741494aaa41541494ce0c5b1c201990d175f0e4abd6cfa4ddfefa9aab0a1")
elseif(CONFIG STREQUAL "Release")
    set(expected_census_sha256 "285273abaf31a916d0fbebaaea2131b09f35ae595918ed53a066d7d2045f169b")
else()
    message(FATAL_ERROR "Unsupported Four-Star Wall-Pocket matching configuration: ${CONFIG}")
endif()
set(expected_trace_sha256 "6105daa60a740fc1d9f9a1b9e421a1dc9c87fe980e8aec4b804214a12087dea2")
set(expected_call_sha256 "9abc6c7b5a8d2518f5ecc5481bc693917b056659e9d9f0eec95e155e4ee30e24")
set(expected_write_sha256 "4e983a7998075dfa71ddd478cdcab4ed8995ce7c7960ced84ccbc0bd15ff59a3")
set(expected_state_sha256 "6c5946171ec29f2bf46eb93dbe307aa3492fb10fe80361fdaf7075c8225fd49c")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "873:383E:80"
        --work-ram-patch "1234:2D18:05"
        --observe-hook "02:4A93"
        --observe-hook "02:4AD2"
        --observe-hook "02:4AE5"
        --observe-hook "02:4AEB"
        --replace-hook "02:4A93@all"
        --replace-hook "02:4AD2@all"
        --replace-hook "02:4AE5@all"
        --replace-hook "02:4AEB@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2CFA-2CFA"
        --work-ram-write-range "2CC0-2CC0"
        --work-ram-write-range "2D00-2D01"
        --work-ram-write-range "2D18-2D18"
        --work-ram-write-range "2D33-2D33"
        --work-ram-write-range "303E-303E"
        --work-ram-write-range "373E-373E"
        --work-ram-write-range "3780-3780"
        --work-ram-write-range "383E-383E"
        --work-ram-write-range "393E-393E"
        --work-ram-write-range "3A3E-3A3E"
        --work-ram-write-range "3C3E-3C3E"
        --work-ram-write-range "3D3E-3D3E"
        --work-ram-write-range "3E3E-3E3E"
        --work-ram-write-range "3F3E-3F3E"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Four-Star Wall-Pocket matching recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 2"
    "Native replacement 02:4A93@all entries: 1168 of 1168"
    "Native replacement 02:4AD2@all entries: 1 of 1"
    "Native replacement 02:4AE5@all entries: 1 of 1"
    "Native replacement 02:4AEB@all entries: 506 of 506"
    "Observe hook 1 entries: 1168"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 506"
    "Routine trace complete: 1676 entries"
    "Routine call trace complete: 1676 calls, 3338 writes"
    "Work-RAM write trace complete: 789 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Four-Star Wall-Pocket matching output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Four-Star Wall-Pocket matching ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Four-Star Wall-Pocket matching artifacts verified: ${actual_census_sha256}, ${actual_trace_sha256}, ${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}")
