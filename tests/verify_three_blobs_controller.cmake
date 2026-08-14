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
        "Unsupported Three-Blobs-controller configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "78b471bd149b91424ea4a3f8fda735e4a1e84a6eb2a248c1a4ab58ebde911896"
)
set(
    expected_call_sha256
    "c8ad96ed335ed5fe890119d1591eadd0598e6f9efd8a5e135ca445848ca74ed6"
)
set(
    expected_write_sha256
    "c834a7894530277d04cde21a1d3d3d17caf2ee44180f181f8906db8e12874dff"
)
set(
    expected_state_sha256
    "2079deeea652e94c820715c3f9f4ad805c49a47da281991642c422b3d4d69f8e"
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
        "03:65BC"
        --observe-hook
        "03:65C2"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:65C2@1"
        --work-ram-write-range
        "2053-2053"
        --work-ram-write-range
        "222A-222B"
        --work-ram-write-range
        "2254-2254"
        --work-ram-write-range
        "2CFD-2CFD"
        --work-ram-write-range
        "2D39-2D3C"
        --work-ram-write-range
        "3057-3057"
        --work-ram-write-range
        "30D7-30D7"
        --work-ram-write-range
        "3857-3857"
        --work-ram-write-range
        "38D7-38D7"
        --work-ram-write-range
        "3957-3957"
        --work-ram-write-range
        "39D7-39D7"
        --work-ram-write-range
        "3A57-3A57"
        --work-ram-write-range
        "3AD7-3AD7"
        --work-ram-write-range
        "3C57-3C57"
        --work-ram-write-range
        "3CD7-3CD7"
        --work-ram-write-range
        "3D57-3D5A"
        --work-ram-write-range
        "3DD7-3DDA"
        --work-ram-write-range
        "3E57-3E57"
        --work-ram-write-range
        "3ED7-3ED7"
        --work-ram-write-range
        "3F57-3F57"
        --work-ram-write-range
        "3FD7-3FD7"
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
        "Three-Blobs controller recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 2650"
    "Routine trace complete: 2651 entries"
    "Routine call trace complete: 2651 calls, 673 writes"
    "Routine replay 03:65C2@1 verified: frame 873"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 9366 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Three-Blobs controller output omitted '${expected_line}'.\n"
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
            "Three-Blobs controller ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Three-Blobs controller ${artifact} verified: ${actual_sha256}"
    )
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-ThreeBlobsControllerSemantics.ps1"
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
        "Three-Blobs controller analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Three-Blobs controller semantics verified."
    "Fixed controller: slot 57 installs 03:65C2"
    "Routed outcomes: 2,565 first-pair mismatches"
    "Inactive result contract: all-red awards 100 stored units"
    "First normalized sample: 03:65C2@1, frame 873"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Three-Blobs controller semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
