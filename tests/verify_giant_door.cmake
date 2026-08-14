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
        "Unsupported Giant-Door-test configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "d5f63706bf0312cb32d0116471c24e5f11955f10a7aad1bacfefb8e6650e947e"
)
set(
    expected_call_sha256
    "e329e730c8f8bbbd9d4b54092b6177907fb333378d5c4853dd40ca55107e5419"
)
set(
    expected_write_sha256
    "d4fb4aabcf69de8c9b6dd89349ffe4afbc61dc55af4197aedb48fe69ba7ed05e"
)
set(
    expected_state_write_sha256
    "64e95fd177928acbb6416a3048b3ceb6c12babde77d3279c14773697e5774cdc"
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
        "03:6294"
        --observe-hook
        "03:62B8"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:62B8@1"
        --work-ram-write-range
        "2D06-2D0A"
        --work-ram-write-range
        "304C-304C"
        --work-ram-write-range
        "30CC-30CC"
        --work-ram-write-range
        "384C-384C"
        --work-ram-write-range
        "38CC-38CC"
        --work-ram-write-range
        "394C-394C"
        --work-ram-write-range
        "39CC-39CC"
        --work-ram-write-range
        "3A4C-3A4C"
        --work-ram-write-range
        "3B4C-3B4C"
        --work-ram-write-range
        "3C4C-3C4C"
        --work-ram-write-range
        "3CCC-3CCC"
        --work-ram-write-range
        "3D4C-3D4C"
        --work-ram-write-range
        "3DCC-3DCC"
        --work-ram-write-range
        "3F4C-3F4C"
        --work-ram-write-range
        "3FCC-3FCC"
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
        "Giant-Door recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 676"
    "Routine trace complete: 677 entries"
    "Routine call trace complete: 677 calls, 103 writes"
    "Routine replay 03:62B8@1 verified: frame 1228"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 579 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant-Door output omitted '${expected_line}'.\n"
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
            "Giant-Door ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Giant-Door ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Giant-Door selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Giant-Door selected state verified: ${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-GiantDoorSemantics.ps1"
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
        "Giant-Door semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Giant-Door semantics verified."
    "Fixed slot 4C at (64,254) maps initializer 03:6294"
    "676 steady calls take the no-hit return"
    "ROM-bounded hits award 50 stored units"
    "Normalized sample: 03:62B8@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant-Door semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
