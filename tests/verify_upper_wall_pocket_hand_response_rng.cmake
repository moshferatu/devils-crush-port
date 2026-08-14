# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "f05eece598fa412b212a9bfb85b380c3239bb0e87b0906b7970b6ea0591fbb02"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "911b75168bfe006c674b96dfccfef3d181f85f4d969cd310567639cc37f2cb26"
    )
else()
    message(
        FATAL_ERROR
        "Unsupported upper-hand-response-RNG configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "495edd1b805960b6e13d581512bfc002aa402c8c1e28d6126b19b2b62a1521d0"
)
set(
    expected_call_sha256
    "0dd96f48548eca800a304198bd9b5f435c89cc57da4c35a999e924172ecf6454"
)
set(
    expected_write_sha256
    "3be62639e568238b02878718fd0c5396a21c04b0fadd40327414bee0a8ad7932"
)
set(
    expected_state_sha256
    "678b920696aeb42bab4cfd463a371fe1d212e54f1e9845606ba2f1769572e5a8"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:3340:2C"
        --work-ram-patch "1200:33C0:01"
        --work-ram-patch "1200:3F40:F5"
        --work-ram-patch "1200:3FC0:59"
        --observe-hook "02:59F5"
        --observe-hook "00:E8B3"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "3040-3040"
        --work-ram-write-range "30C0-30C0"
        --work-ram-write-range "3A40-3A40"
        --work-ram-write-range "3AC0-3AC0"
        --work-ram-write-range "3B40-3B40"
        --work-ram-write-range "3BC0-3BC0"
        --work-ram-write-range "3DC0-3DC0"
        --work-ram-write-range "3F40-3F40"
        --work-ram-write-range "3FC0-3FC0"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Upper-hand response RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 4"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 23"
    "Routine trace complete: 24 entries"
    "Routine call trace complete: 24 calls, 260 writes"
    "Work-RAM write trace complete: 4493 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Upper-hand response RNG output omitted '${expected_line}'.\n"
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
    set("actual_${artifact}_sha256" "${actual_sha256}")
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Upper-hand response RNG ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-UpperWallPocketHandResponseRngSemantics.ps1"
        -RomPath "${ROM}"
        -CallPath "${call_output}"
        -WritePath "${write_output}"
        -StatePath "${state_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Upper-hand response RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Upper Wall-Pocket hand response RNG semantics verified."
    "Activation: fixed slot 40 enters response state 02:59F5 at frame 1221."
    "RNG consumer 02:59F5: result 73 maps to response E3."
    "Response continuation: 02:5A00 counts E3 to zero through frame 1447."
    "Cycle return: the reverse pass reaches 02:597D at frame 1450."
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Upper-hand response RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(
    STATUS
    "Upper-hand response RNG artifacts verified: "
    "${actual_census_sha256}, ${actual_trace_sha256}, "
    "${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}\n"
    "${semantics_output}"
)
