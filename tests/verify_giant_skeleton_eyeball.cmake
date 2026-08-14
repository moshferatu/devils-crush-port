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
        "Unsupported Giant-Skeleton-Eyeball configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "eaaf71693e8b30d80c1d8f0e78870f195788d8b1d00bcd18ddb289d0f9108dd5"
)
set(
    expected_call_sha256
    "b3c372034c15d85d66500392b79bf5e28047d0cfdb4a2766d8f2b8e9c8cd1cd2"
)
set(
    expected_write_sha256
    "dc8a4ef320c2acaa1ddf0601f0bbaaff69c83898e48da804b72e1243d9ddea4f"
)
set(
    expected_state_write_sha256
    "7d5d7067ee92eeb9c25882867ffcb3b6b91824d2dea673444fe32a2517112091"
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
        "02:4DC8"
        --observe-hook
        "02:4DD9"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:4DD9@1"
        --work-ram-write-range
        "2D20-2D20"
        --work-ram-write-range
        "2D4F-2D53"
        --work-ram-write-range
        "2255-2257"
        --work-ram-write-range
        "302F-302F"
        --work-ram-write-range
        "30AF-30AF"
        --work-ram-write-range
        "382F-382F"
        --work-ram-write-range
        "38AF-38AF"
        --work-ram-write-range
        "392F-392F"
        --work-ram-write-range
        "39AF-39AF"
        --work-ram-write-range
        "3A2F-3A2F"
        --work-ram-write-range
        "3AAF-3AAF"
        --work-ram-write-range
        "3D2F-3D2F"
        --work-ram-write-range
        "3DAF-3DAF"
        --work-ram-write-range
        "3E2F-3E2F"
        --work-ram-write-range
        "3EAF-3EAF"
        --work-ram-write-range
        "3F2F-3F2F"
        --work-ram-write-range
        "3FAF-3FAF"
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
        "Giant-Skeleton-Eyeball recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1988"
    "Routine trace complete: 1989 entries"
    "Routine call trace complete: 1989 calls, 503 writes"
    "Routine replay 02:4DD9@1 verified: frame 873, 57 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 596 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant-Skeleton-Eyeball output omitted '${expected_line}'.\n"
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
            "Giant-Skeleton-Eyeball ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Giant-Skeleton-Eyeball ${artifact} verified: ${actual_sha256}"
    )
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Giant-Skeleton-Eyeball selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Giant-Skeleton-Eyeball selected state verified: "
    "${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-GiantSkeletonEyeballSemantics.ps1"
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
        "Giant-Skeleton-Eyeball semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Giant-Skeleton-Eyeball semantics verified."
    "Fixed slot 2F at (225,535) maps initializer 02:4DC8"
    "1988 steady calls remain dormant"
    "Dragon's-Egg count zero sets 2D20 bit 0"
    "Normalized sample: 02:4DD9@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant-Skeleton-Eyeball semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
