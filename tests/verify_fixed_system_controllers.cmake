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
        "Unsupported fixed-system-controller configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "fade5f879a62c2e9aadcac7bed74374c2ede950de526632a72e0c611db619d7b"
)
set(
    expected_call_sha256
    "da3d925ec68f9295224ccec32fe500708b17f09404e4f8f99b2527a8cd736261"
)
set(
    expected_write_sha256
    "66e67562ba3f7702c485ab0ff10d0a3d0b84a24b1116c1aae2834c99e1e96588"
)
set(
    expected_state_sha256
    "22e191e0f6dfb327096c30089d63f26c08a1e677cef0ffe0a41a376a7058f450"
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
        "03:7B1F"
        --observe-hook
        "03:7B27"
        --observe-hook
        "03:7B6D"
        --observe-hook
        "03:7B7A"
        --observe-hook
        "03:7CA7"
        --observe-hook
        "03:7CAF"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:7B27@1"
        --routine-replay
        "03:7B7A@1"
        --routine-replay
        "03:7CAF@1"
        --work-ram-write-range
        "2053-2053"
        --work-ram-write-range
        "2BB9-2BB9"
        --work-ram-write-range
        "2CC0-2CC0"
        --work-ram-write-range
        "2D38-2D38"
        --work-ram-write-range
        "305C-305E"
        --work-ram-write-range
        "30DC-30DE"
        --work-ram-write-range
        "385C-385E"
        --work-ram-write-range
        "395C-395E"
        --work-ram-write-range
        "39DC-39DE"
        --work-ram-write-range
        "3A5C-3A5E"
        --work-ram-write-range
        "3ADC-3ADE"
        --work-ram-write-range
        "3B5C-3B5E"
        --work-ram-write-range
        "3BDC-3BDE"
        --work-ram-write-range
        "3D5C-3D5E"
        --work-ram-write-range
        "3DDC-3DDE"
        --work-ram-write-range
        "3E5C-3E5E"
        --work-ram-write-range
        "3EDC-3EDE"
        --work-ram-write-range
        "3F5C-3F5E"
        --work-ram-write-range
        "3FDC-3FDE"
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
        "Fixed system-controller recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 2650"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 2650"
    "Observe hook 5 entries: 1"
    "Observe hook 6 entries: 2650"
    "Routine trace complete: 7953 entries"
    "Routine call trace complete: 7953 calls, 135845 writes"
    "Routine replay 03:7B27@1 verified: frame 873, 48 body cycles"
    "Routine replay 03:7B7A@1 verified: frame 873, 48 body cycles"
    "Routine replay 03:7CAF@1 verified: frame 873, 45 body cycles"
    "Routine replay complete: 3 state-normalized calls"
    "Work-RAM write trace complete: 8381 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Fixed system-controller output omitted '${expected_line}'.\n"
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
            "Fixed system-controller ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Fixed system-controller ${artifact} verified: ${actual_sha256}"
    )
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-FixedSystemControllerSemantics.ps1"
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
        "Fixed system-controller analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Fixed system-controller semantics verified."
    "Hidden records: slots 5C-5E install 03:7B27, 03:7B7A, and 03:7CAF."
    "Eight-frame presentation cadence: 332 triple export submissions"
    "Marker phase table: 0-1-2-3-4-3-2-1."
    "Collision response: slot 5E remains dormant and 2D38 stays zero."
    "First normalized samples: 03:7B27@1 = 48 cycles"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Fixed system-controller semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
