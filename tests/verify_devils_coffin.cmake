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
        "Unsupported Devil's-Coffin-test configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "d98bad357038cec227531a34c354103eec68890c1f0a73795e17af7bcaf6d4fa"
)
set(
    expected_call_sha256
    "5ca7ca788988c56bb24209faa87a4f08ece30a9be5148a34cf7f5c0ab05a6888"
)
set(
    expected_write_sha256
    "9ae1c018ec68f77c49a2188b6e0950527ba70f6c13cd5ea2260f79873284068c"
)
set(
    expected_state_sha256
    "1c3fd4bac31662487cb5c48b2389a4584526bd567a42fdcc587bf5d0cbfe3169"
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
        "03:63E1"
        --observe-hook
        "03:63F1"
        --observe-hook
        "03:640A"
        --observe-hook
        "03:6432"
        --observe-hook
        "03:6445"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:63F1@1"
        --routine-replay
        "03:640A@1"
        --routine-replay
        "03:6432@1"
        --routine-replay
        "03:6445@1"
        --work-ram-write-range
        "2053-2053"
        --work-ram-write-range
        "300F-300F"
        --work-ram-write-range
        "380F-380F"
        --work-ram-write-range
        "390F-390F"
        --work-ram-write-range
        "398F-398F"
        --work-ram-write-range
        "3A0F-3A0F"
        --work-ram-write-range
        "3F0F-3F0F"
        --work-ram-write-range
        "3F8F-3F8F"
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
        "Devil's Coffin recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 3"
    "Observe hook 2 entries: 643"
    "Observe hook 3 entries: 18"
    "Observe hook 4 entries: 2"
    "Observe hook 5 entries: 18"
    "Routine trace complete: 684 entries"
    "Routine call trace complete: 684 calls, 355 writes"
    "Work-RAM write trace complete: 372 selected writes"
    "Routine replay 03:63F1@1 verified: frame 1227"
    "Routine replay 03:640A@1 verified: frame 1245"
    "Routine replay 03:6432@1 verified: frame 1254"
    "Routine replay 03:6445@1 verified: frame 1255"
    "Routine replay complete: 4 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Devil's Coffin output omitted '${expected_line}'.\n"
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
            "Devil's Coffin ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Devil's Coffin ${artifact} verified: ${actual_sha256}"
    )
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-DevilsCoffinSemantics.ps1"
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
        "Devil's Coffin analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Devil's Coffin semantics verified."
    "Fixed return-gate target: slot 0F at (221,257)"
    "Routed contacts: frames 1244 and 1297"
    "Completion clears session bit $53.3 and changes type 83 to 82"
    "Normalized samples: 03:63F1/640A/6432/6445"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Devil's Coffin semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
