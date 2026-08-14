# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "527e8b3f876703cc5444a3432e8dae8ada1e8a848a93e5e847193675c262c4b3"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "8885978460f9f0c234ad13a27b113818e32de5c12cfbb84a31c56b27867d5b61"
    )
else()
    message(
        FATAL_ERROR
        "Unsupported Three-Blobs-reset-RNG configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "963523a50df91c1197b33a2111ce52b55e344ed5ea1c2f1bdd5f18f400cf9290"
)
set(
    expected_call_sha256
    "f2980b1b369d5adc3c8c837a5349d597979cfa25384ddb93dabb848c31ccdc37"
)
set(
    expected_write_sha256
    "25c575c6ccfe1939c9886452abc7bbb2212a810ffeb4e09391454c298d232ebb"
)
set(
    expected_state_sha256
    "96b9bd72a8a5343841098aa86ae1ed498ede65f5d6a9e38b291f65a079d74aa6"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:2D3A:00"
        --work-ram-patch "1200:2D3B:00"
        --observe-hook "03:65DE"
        --observe-hook "03:661B"
        --observe-hook "03:6620"
        --observe-hook "00:E8B3"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "03:661B@1"
        --routine-replay "03:6620@1"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "2CC0-2CC0"
        --work-ram-write-range "2D39-2D3C"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Three-Blobs reset RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 2"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 23"
    "Routine trace complete: 26 entries"
    "Routine call trace complete: 26 calls, 292 writes"
    "Work-RAM write trace complete: 3695 selected writes"
    "Routine replay 03:661B@1 verified: frame 1200, 774 body cycles"
    "Routine replay 03:6620@1 verified: frame 1200, 753 body cycles"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Three-Blobs reset RNG output omitted '${expected_line}'.\n"
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
            "Three-Blobs reset RNG ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-ThreeBlobsResetRngSemantics.ps1"
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
        "Three-Blobs reset RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Three-Blobs reset RNG semantics verified."
    "Equality route: aligned state zero enters 03:65DE at frame 1200."
    "Outcome: state zero submits 10 stored score units before reset."
    "RNG consumer 03:6620: result 5D produces consecutive states 1/2/3."
    "Normalized reset samples: 03:661B@1 is 774 cycles"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Three-Blobs reset RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(
    STATUS
    "Three-Blobs reset RNG artifacts verified: "
    "${actual_census_sha256}, ${actual_trace_sha256}, "
    "${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}\n"
    "${semantics_output}"
)
