# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
    )
else()
    message(
        FATAL_ERROR
        "Unsupported upper-Wall-Pocket-hand configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "defcf166fa1b1bf77b4e415f57d082f06f60d73d8f73afbbad036b46713062d9"
)
set(
    expected_call_sha256
    "98595fe928d45a59437f77f804b662a67f1860c2358c649c109b48d2d420ed92"
)
set(
    expected_write_sha256
    "1f7ce0a73fc4e1c5c9dcc3953563ea48def6dfa37b482d9e827a57cbe23f3490"
)
set(
    expected_state_sha256
    "e4521a0c6afa7d98deba96c4f21032e2ff7e702629371a2ac07b9c097bd3dc0b"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "02:5972"
        --replace-hook
        "02:5972@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:5972@1"
        --work-ram-write-range
        "2D33-2D33"
        --work-ram-write-range
        "3040-3041"
        --work-ram-write-range
        "30C0-30C1"
        --work-ram-write-range
        "3840-3841"
        --work-ram-write-range
        "3940-3941"
        --work-ram-write-range
        "39C0-39C1"
        --work-ram-write-range
        "3A40-3A41"
        --work-ram-write-range
        "3AC0-3AC1"
        --work-ram-write-range
        "3B40-3B41"
        --work-ram-write-range
        "3BC0-3BC1"
        --work-ram-write-range
        "3E40-3E41"
        --work-ram-write-range
        "3EC0-3EC1"
        --work-ram-write-range
        "3F40-3F41"
        --work-ram-write-range
        "3FC0-3FC1"
        --work-ram-write-trace
        "${state_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Upper Wall-Pocket hand recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 2"
    "Native replacement 02:5972@all entries: 2 of 2"
    "Routine trace complete: 2 entries"
    "Routine call trace complete: 2 calls, 14 writes"
    "Routine replay 02:5972@1 verified: frame 872, 192 body cycles"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 1129 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Upper Wall-Pocket hand output omitted '${expected_line}'.\n"
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
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Upper Wall-Pocket hand ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Upper Wall-Pocket hand ${artifact} verified: ${actual_sha256}"
    )
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-UpperWallPocketHandSemantics.ps1"
        -RomPath
        "${ROM}"
        -CallPath
        "${call_output}"
        -WritePath
        "${write_output}"
        -StatePath
        "${state_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Upper Wall-Pocket hand analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Upper Wall-Pocket hand semantics verified."
    "Fixed mirrored hands: slots 40/41"
    "ROM-bounded gate: 2D33 selects the upper pair"
    "Routed coverage: two frame-872 initializers"
    "The hand family owns no score or audio command."
    "First normalized sample: 02:5972@1, frame 872, 192 cycles"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Upper Wall-Pocket hand semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
