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
    message(FATAL_ERROR "Unsupported Mercy-Star configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "1843ecb62f1d3c7575aea3d44e19aadb145a4a7bfe24dfba3b1f1c1b6fd9c291"
)
set(
    expected_call_sha256
    "18c7eaacc8eb696d68fe37519de54ab04823e3c0edc1854a74644de1cb7528c1"
)
set(
    expected_write_sha256
    "444d3b49380e4ac58eafc96fac130f6e8a757165fa304347087e988cda444eab"
)
set(
    expected_state_write_sha256
    "62528a08094cea8cea98955e821398c75e0477d6e04aa5d2460ccdfccbd17fc6"
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
        "02:4F9F"
        --observe-hook
        "02:4FC1"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:4FC1@1"
        --work-ram-write-range
        "222D-222D"
        --work-ram-write-range
        "2251-2251"
        --work-ram-write-range
        "2254-2254"
        --work-ram-write-range
        "2D52-2D52"
        --work-ram-write-range
        "305F-3061"
        --work-ram-write-range
        "30DF-30E1"
        --work-ram-write-range
        "385F-3861"
        --work-ram-write-range
        "395F-3961"
        --work-ram-write-range
        "39DF-39E1"
        --work-ram-write-range
        "3A5F-3A61"
        --work-ram-write-range
        "3C5F-3C61"
        --work-ram-write-range
        "3CDF-3CE1"
        --work-ram-write-range
        "3E5F-3E61"
        --work-ram-write-range
        "3EDF-3EE1"
        --work-ram-write-range
        "3F5F-3F61"
        --work-ram-write-range
        "3FDF-3FE1"
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
        "Mercy-Star recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 3"
    "Observe hook 2 entries: 7950"
    "Routine trace complete: 7953 entries"
    "Routine call trace complete: 7953 calls, 2206 writes"
    "Routine replay 02:4FC1@1 verified: frame 873, 54 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 1696 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Mercy-Star output omitted '${expected_line}'.\n"
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
            "Mercy-Star ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Mercy-Star ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Mercy-Star selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Mercy-Star selected state verified: ${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-MercyStarSemantics.ps1"
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
        "Mercy-Star semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Mercy-Star semantics verified."
    "Fixed slots 5F-61 at (132/148/164,660)"
    "7950 steady calls remain dormant"
    "activation requires the last ball and a score below 500000"
    "Normalized sample: 02:4FC1@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Mercy-Star semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
