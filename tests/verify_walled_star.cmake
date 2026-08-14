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
        "Unsupported Walled-Star-test configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "f8005a81e344b625a955dee6d8cbccb3a03b3ad5795099d48b171f674ab360c2"
)
set(
    expected_call_sha256
    "6c39d552345dbcc6a7dc434613a6a2d789eb8fa67db15f1ce88d52a3c88f0109"
)
set(
    expected_write_sha256
    "7811e173ba484531f7545ca7dcf77d268216d0073aca628b5f689caa8baecc04"
)
set(
    expected_state_write_sha256
    "1e222fd4e49400bd45aaff1a23fb113b73fdb9240e60ba424c3ed4d12b03f14c"
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
        "03:6319"
        --observe-hook
        "03:6339"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:6339@1"
        --work-ram-write-range
        "2D32-2D32"
        --work-ram-write-range
        "2D53-2D53"
        --work-ram-write-range
        "2DBC-2DBC"
        --work-ram-write-range
        "305B-305B"
        --work-ram-write-range
        "30DB-30DB"
        --work-ram-write-range
        "385B-385B"
        --work-ram-write-range
        "38DB-38DB"
        --work-ram-write-range
        "395B-395B"
        --work-ram-write-range
        "39DB-39DB"
        --work-ram-write-range
        "3A5B-3A5B"
        --work-ram-write-range
        "3D5B-3D5B"
        --work-ram-write-range
        "3F5B-3F5B"
        --work-ram-write-range
        "3FDB-3FDB"
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
        "Walled-Star recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 2650"
    "Routine trace complete: 2651 entries"
    "Routine call trace complete: 2651 calls, 671 writes"
    "Routine replay 03:6339@1 verified: frame 873"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 568 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Walled-Star output omitted '${expected_line}'.\n"
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
            "Walled-Star ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Walled-Star ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Walled-Star selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Walled-Star selected state verified: ${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-WalledStarSemantics.ps1"
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
        "Walled-Star semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Walled-Star semantics verified."
    "Fixed slot 5B at (32,302) maps initializer 03:6319"
    "2650 steady calls remain unhit"
    "ROM-bounded wall damage uses four 50-unit hits"
    "Normalized sample: 03:6339@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Walled-Star semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
