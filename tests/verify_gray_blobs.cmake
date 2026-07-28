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
        "Unsupported Gray-Blob-test configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "75aa6fc29520a22fe188cb81242b65e9a504d36b81756b83f9865e55cf89d8b0"
)
set(
    expected_call_sha256
    "d8d4381418800d61f2e06b4d458b1f3a9b37d8dc74081e1d6866703c700420e8"
)
set(
    expected_write_sha256
    "9b5c4d13a3bd3fcaa229ef2b23fa0094c7fa57af9ec5eb898ace6e19ded38193"
)
set(
    expected_state_sha256
    "7b5c9ebeb29c3784227aa6ccf6c367bb9c5f57a27126777022c1142dfae0ca6a"
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
        "03:6876"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:6876@1"
        --work-ram-write-range
        "2CFD-2CFD"
        --work-ram-write-range
        "3050-3056"
        --work-ram-write-range
        "3350-3356"
        --work-ram-write-range
        "33D0-33D6"
        --work-ram-write-range
        "3450-3456"
        --work-ram-write-range
        "34D0-34D6"
        --work-ram-write-range
        "3650-3656"
        --work-ram-write-range
        "3850-3856"
        --work-ram-write-range
        "3A50-3A56"
        --work-ram-write-range
        "3AD0-3AD6"
        --work-ram-write-range
        "3B50-3B56"
        --work-ram-write-range
        "3BD0-3BD6"
        --work-ram-write-range
        "3D50-3D56"
        --work-ram-write-range
        "3E50-3E56"
        --work-ram-write-range
        "3ED0-3ED6"
        --work-ram-write-range
        "3F50-3F56"
        --work-ram-write-range
        "3FD0-3FD6"
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
        "Gray-Blob recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 7"
    "Routine trace complete: 7 entries"
    "Routine call trace complete: 7 calls, 105 writes"
    "Routine replay 03:6876@1 verified: frame 872, 372 body cycles"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 2439 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Gray-Blob output omitted '${expected_line}'.\n"
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
            "Gray-Blob ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Gray-Blob ${artifact} verified: ${actual_sha256}")
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-GrayBlobSemantics.ps1"
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
        "Gray-Blob analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Gray-Blob semantics verified."
    "Fixed upper-chute row: slots 50-56"
    "Routed coverage: seven frame-872 initializers"
    "Inactive hit contract: 03E8 stored units"
    "Three-Blobs coupling: 2CFD selects alternate placement at 03:67EC"
    "First normalized sample: 03:6876@1, frame 872, 372 cycles"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Gray-Blob semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
