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
    message(FATAL_ERROR "Unsupported RNG-test configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "a237a4b45a7be1097c03b78722061b6a151247b1e82706470d155cf09cdd2815"
)
set(
    expected_call_sha256
    "ca8f829fe44fdcbfd0d85144db6b16aa8d973e4b5c04169ed959325dee01f218"
)
set(
    expected_write_sha256
    "06389c11544b16f5a98b6b509b124533cb9e0b57858c274471faf6eee506eb7e"
)
set(
    expected_state_write_sha256
    "61bf739045460c23feb39498cc48c09d000cccb888bd0fe0110b897b50f66952"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_write_output "${OUTPUT}.state-writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "00:E8B3"
        --replace-hook
        "00:E8B3@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "00:E8B3@1"
        --work-ram-write-range
        "222A-222B"
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
        "RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:E8B3@all entries: 78 of 78"
    "Observe hook 1 entries: 78"
    "Routine trace complete: 78 entries"
    "Routine call trace complete: 78 calls, 703 writes"
    "Work-RAM write trace complete: 3771 selected writes"
    "Routine replay 00:E8B3@1 verified: frame 1228"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "RNG output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write state_write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${state_write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "RNG ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "RNG ${artifact} verified: ${actual_sha256}")
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-RngSemantics.ps1"
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
        "RNG semantics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "RNG semantics verified."
    "E8B3: 78 calls follow the exact eight-write recurrence"
    "Active RNG consumers: 6678=9 calls; 6697=69 calls."
    "RNG consumer census: 21 sites; 12 normal-table and 9 bonus-stage."
    "Consumer transforms: 12 masked timers/offsets, 6 binary selectors"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "RNG semantics output omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
message(
    STATUS
    "Semantic E8B3 replacement covers all 78 calls with checkpointed interrupts."
)
