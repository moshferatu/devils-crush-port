# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "cf4c90d01c8bddc8109f0d48503f0a0713e3f7ea71f19c286f8091d0e1ff709a"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "7af61a496eb4584381b91dd56a611732361bb115e3c1cbc6110cbdcc5664e0a8"
    )
else()
    message(
        FATAL_ERROR
        "Unsupported Little-Skull-RNG configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "5d82dc538f803bcbca0d57f15dc8a4987ee77a9fb88236bc28b08e85a9d34a6d"
)
set(
    expected_call_sha256
    "943a683be718416d5e6198497304494c3bb226f9a47d43b43faf814fbf1f4660"
)
set(
    expected_write_sha256
    "3c130ad07bd06f3e7bdef0f4847101963fd0493955acaae71316c076145e30bf"
)
set(
    expected_state_sha256
    "9de9b281b871dfc270635d6c3aa9b017bbd1f97b146a0f7c6c00d7d335216290"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:3346:2C"
        --work-ram-patch "1200:33C6:01"
        --work-ram-patch "1200:3F46:8A"
        --work-ram-patch "1200:3FC6:45"
        --work-ram-patch "1222:3346:2C"
        --work-ram-patch "1222:33C6:01"
        --observe-hook "02:458A"
        --replace-hook "02:458A@all"
        --observe-hook "02:45A4"
        --observe-hook "02:45C9"
        --observe-hook "00:E8B3"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "02:458A@1"
        --routine-replay "02:45A4@1"
        --routine-replay "02:45C9@1"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "2CFA-2CFA"
        --work-ram-write-range "2D04-2D04"
        --work-ram-write-range "3046-3046"
        --work-ram-write-range "30C6-30C6"
        --work-ram-write-range "3346-3346"
        --work-ram-write-range "33C6-33C6"
        --work-ram-write-range "3846-3846"
        --work-ram-write-range "38C6-38C6"
        --work-ram-write-range "3946-3946"
        --work-ram-write-range "39C6-39C6"
        --work-ram-write-range "3A46-3A46"
        --work-ram-write-range "3AC6-3AC6"
        --work-ram-write-range "3B46-3B46"
        --work-ram-write-range "3BC6-3BC6"
        --work-ram-write-range "3D46-3D46"
        --work-ram-write-range "3F46-3F46"
        --work-ram-write-range "3FC6-3FC6"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Little-Skull RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 6"
    "Native replacement 02:458A@all entries: 1 of 1"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 128"
    "Routine trace complete: 131 entries"
    "Routine call trace complete: 131 calls, 1268 writes"
    "Work-RAM write trace complete: 5274 selected writes"
    "Routine replay 02:458A@1 verified: frame 1221, 291 body cycles, 11 ordered writes"
    "Routine replay 02:45A4@1 verified: frame 1223, 768 body cycles"
    "Routine replay 02:45C9@1 verified: frame 1223, 606 body cycles"
    "Routine replay complete: 3 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Little-Skull RNG output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
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
        message(
            FATAL_ERROR
            "Little-Skull RNG ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-LittleSkullRngSemantics.ps1"
        -RomPath "${ROM}"
        -CallPath "${call_output}"
        -WritePath "${write_output}"
        -StatePath "${state_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Little-Skull RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Little-Skull RNG semantics verified."
    "Activation: fixed slot 46 enters gray state 02:458A at frame 1221."
    "RNG consumer 02:45C9: result 75 selects left-pair pending value 2."
    "Call contract: release 02:45A4 is 768 cycles"
    "Normalized samples: 02:45A4@1 and 02:45C9@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Little-Skull RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(
    STATUS
    "Little-Skull RNG artifacts verified: "
    "${actual_census_sha256}, ${actual_trace_sha256}, "
    "${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}\n"
    "${semantics_output}"
)
