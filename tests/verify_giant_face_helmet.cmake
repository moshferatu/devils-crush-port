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
        "Unsupported Giant-Face-helmet-test configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "fb473f9da4c43e0eea2abcfed06c5510b3d2c6a457bcbd6e791c5b91be6cb057"
)
set(
    expected_call_sha256
    "48a1a89f272e80679e859d6ecdc34d4102d864985fba5ba64bdde12d0642e64b"
)
set(
    expected_write_sha256
    "1d70d3313ad65639066692b2721f1df8d5ffc4d041e5625a3f84fceaf3dcd4fa"
)
set(
    expected_state_write_sha256
    "6692ff8cb8ddb9927b57e2844dea70ea8c11afc48a3ac18707350690b83c5d26"
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
        "02:4A5C"
        --observe-hook
        "02:4A64"
        --observe-hook
        "02:4C78"
        --observe-hook
        "02:4C91"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:4C91@1"
        --work-ram-write-range
        "2258-2258"
        --work-ram-write-range
        "2D0C-2D0D"
        --work-ram-write-range
        "2D16-2D17"
        --work-ram-write-range
        "302A-302B"
        --work-ram-write-range
        "30AA-30AB"
        --work-ram-write-range
        "382A-382B"
        --work-ram-write-range
        "392A-392B"
        --work-ram-write-range
        "39AA-39AB"
        --work-ram-write-range
        "3A2A-3A2B"
        --work-ram-write-range
        "3C2A-3C2B"
        --work-ram-write-range
        "3CAA-3CAB"
        --work-ram-write-range
        "3D2A-3D2B"
        --work-ram-write-range
        "3DAA-3DAB"
        --work-ram-write-range
        "3E2A-3E2B"
        --work-ram-write-range
        "3EAA-3EAB"
        --work-ram-write-range
        "3F2A-3F2B"
        --work-ram-write-range
        "3FAA-3FAB"
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
        "Giant-Face helmet recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

set(expected_hook_counts 1 1 2 1748)
set(hook_index 1)
foreach(expected_count IN LISTS expected_hook_counts)
    set(expected_line "Observe hook ${hook_index} entries: ${expected_count}")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant-Face helmet output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    math(EXPR hook_index "${hook_index} + 1")
endforeach()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Routine trace complete: 1752 entries"
    "Routine call trace complete: 1752 calls, 3558 writes"
    "Routine replay 02:4C91@1 verified: frame 872"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 3934 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant-Face helmet output omitted '${expected_line}'.\n"
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
            "Giant-Face helmet ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Giant-Face helmet ${artifact} verified: ${actual_sha256}"
    )
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Giant-Face helmet selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Giant-Face helmet selected state verified: ${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-GiantFaceHelmetSemantics.ps1"
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
        "Giant-Face helmet semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Giant-Face helmet-channel semantics verified."
    "Fixed slots 2A/2B are mirrored type-84 helmet channels"
    "1748 idle calls run identical four-tick"
    "ROM-bounded activation sets Giant-Face request bit 0"
    "Normalized sample: 02:4C91@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant-Face helmet semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
