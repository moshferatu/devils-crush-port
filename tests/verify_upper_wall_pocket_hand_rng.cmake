# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "652969878cbd668f6eeaf29bcea3d1fb0aed793583b325f1261f0dc25ab81546"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "652969878cbd668f6eeaf29bcea3d1fb0aed793583b325f1261f0dc25ab81546"
    )
else()
    message(
        FATAL_ERROR
        "Unsupported upper-hand-RNG configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "a51887d0fe4f8a9bd94186384d9df11ed93b998a655bd91aa0706b3de350ffb8"
)
set(
    expected_call_sha256
    "25dcab9ebb6866ce356003e8a9d3eb529535688d2ac025147eb55d790a3b52b8"
)
set(
    expected_write_sha256
    "ac951f417d75a00928fb1dd255030856345542357f0315795f74af753ebbe92d"
)
set(
    expected_state_sha256
    "664cdeb5e46aa4e1f8e2986f9cd915d9dca00456a8c268546e897abe52ebcba5"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:3340:2C"
        --work-ram-patch "1200:33C0:01"
        --observe-hook "02:5975"
        --observe-hook "02:5991"
        --replace-hook "02:5975@all"
        --replace-hook "02:5986@all"
        --replace-hook "02:5991@all"
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
    message(
        FATAL_ERROR
        "Upper Wall-Pocket hand RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 2"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 299"
    "Native replacement 02:5975@all entries: 1 of 1"
    "Native replacement 02:5986@all entries: 1 of 1"
    "Native replacement 02:5991@all entries: 299 of 299"
    "Routine trace complete: 300 entries"
    "Routine call trace complete: 300 calls, 407 writes"
    "Work-RAM write trace complete: 4478 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Upper-hand RNG output omitted '${expected_line}'.\n"
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
            "Upper-hand RNG ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-UpperWallPocketHandRngSemantics.ps1"
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
        "Upper-hand RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Upper Wall-Pocket hand RNG semantics verified."
    "Activation: slot 40 enters the camera window at frame 1220"
    "Native live setup: 02:5975 composes the eight-write RNG recurrence"
    "Countdown: 02:5991 decrements the routed timer through frame 1520."
    "Native countdown: all 299 calls retain the 75/90-cycle timer paths"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Upper-hand RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(
    STATUS
    "Upper-hand RNG artifacts verified: "
    "${actual_census_sha256}, ${actual_trace_sha256}, "
    "${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}\n"
    "${semantics_output}"
)
