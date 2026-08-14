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
    message(FATAL_ERROR "Unsupported Side-Spider configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "c21da02aa95f9e254444e518271a4716a467b2ef45fdb9bf1f847ade20db270e"
)
set(
    expected_call_sha256
    "68a1d6148e713935248b60c93e475409ec6513a1e7aee59cf07dea791556718c"
)
set(
    expected_write_sha256
    "f7b8545de33f6b1fd6c6a597ca2377523a413b7f70502d3460b2d2c792719ea5"
)
set(
    expected_state_write_sha256
    "4a72e3b731fe3913181c59c195431fba9e9f4a95aa2499c1b2497afedcbd3cb9"
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
        "03:64FA"
        --observe-hook
        "03:651F"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:651F@1"
        --work-ram-write-range
        "2D32-2D32"
        --work-ram-write-range
        "3028-3029"
        --work-ram-write-range
        "30A8-30A9"
        --work-ram-write-range
        "3828-3829"
        --work-ram-write-range
        "38A8-38A9"
        --work-ram-write-range
        "3928-3929"
        --work-ram-write-range
        "39A8-39A9"
        --work-ram-write-range
        "3A28-3A29"
        --work-ram-write-range
        "3AA8-3AA9"
        --work-ram-write-range
        "3C28-3C29"
        --work-ram-write-range
        "3CA8-3CA9"
        --work-ram-write-range
        "3D28-3D29"
        --work-ram-write-range
        "3DA8-3DA9"
        --work-ram-write-range
        "3F28-3F29"
        --work-ram-write-range
        "3FA8-3FA9"
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
        "Side-Spider recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 2"
    "Observe hook 2 entries: 2796"
    "Routine trace complete: 2798 entries"
    "Routine call trace complete: 2798 calls, 41649 writes"
    "Routine replay 03:651F@1 verified: frame 873, 903 body cycles, 19 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 7702 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Side-Spider output omitted '${expected_line}'.\n"
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
            "Side-Spider ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Side-Spider ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Side-Spider selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Side-Spider selected state verified: ${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-SideSpiderSemantics.ps1"
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
        "Side-Spider semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Side-Spider semantics verified."
    "Fixed slots 28/29 at (40/240,642)"
    "2796 steady calls split equally"
    "collision 2C/2D, command 34, a 100-unit award"
    "Normalized sample: 03:651F@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Side-Spider semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
