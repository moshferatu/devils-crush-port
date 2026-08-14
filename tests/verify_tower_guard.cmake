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
    message(FATAL_ERROR "Unsupported Tower-Guard configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "f77560857dc0dd46db88679c125b73b3c51652e86b5d2fb87b01c27dec488dd0"
)
set(
    expected_call_sha256
    "4f546f15528cbe203e3406dbd1f0c16d2362350521fa70ba75a1d8cc4d8e9668"
)
set(
    expected_write_sha256
    "c37bed44613fad1cb3f8a8776d182be59e5cd6cd025faa4890832ba9c52bd3e6"
)
set(
    expected_state_write_sha256
    "d66b67b3a2cac7a9f8ec532124a2404a548c7a184a6d2f58030b36e891552f6c"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_write_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "03:6099"
        --observe-hook
        "03:60A6"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:60A6@1"
        --work-ram-write-range
        "2CFC-2CFC"
        --work-ram-write-range
        "2D2D-2D2D"
        --work-ram-write-range
        "2D4D-2D4D"
        --work-ram-write-range
        "302E-302E"
        --work-ram-write-range
        "30AE-30AE"
        --work-ram-write-range
        "382E-382E"
        --work-ram-write-range
        "38AE-38AE"
        --work-ram-write-range
        "392E-392E"
        --work-ram-write-range
        "39AE-39AE"
        --work-ram-write-range
        "3A2E-3A2E"
        --work-ram-write-range
        "3AAE-3AAE"
        --work-ram-write-range
        "3C2E-3C2E"
        --work-ram-write-range
        "3CAE-3CAE"
        --work-ram-write-range
        "3D2E-3D2E"
        --work-ram-write-range
        "3DAE-3DAE"
        --work-ram-write-range
        "3F2E-3F2E"
        --work-ram-write-range
        "3FAE-3FAE"
        --work-ram-write-trace
        "${state_write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Tower-Guard recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 2650"
    "Routine trace complete: 2651 entries"
    "Routine call trace complete: 2651 calls, 3412 writes"
    "Routine replay 03:60A6@1 verified: frame 873, 48 body cycles, 1 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 3230 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Tower-Guard output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Tower-Guard ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Tower-Guard ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Tower-Guard selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Tower-Guard selected state verified: ${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-TowerGuardSemantics.ps1"
        -RomPath
        "${ROM}"
        -CallPath
        "${call_output}"
        -WritePath
        "${write_output}"
        -StateWritePath
        "${state_write_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Tower-Guard semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Tower-Guard semantics verified."
    "Fixed slot 2E at (132,510)"
    "2650 updates keep selector 84"
    "gate zero selects 8E"
    "Normalized sample: 03:60A6@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Tower-Guard semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
