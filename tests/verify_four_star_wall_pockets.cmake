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
        "Unsupported Four-Star-Wall-Pocket-test configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "eafb2abd72e818a258eee861904f0abb097cb1703c885a66316b08a318fad3d9"
)
set(
    expected_call_sha256
    "2f86b84a3997f8a84239a554ecb34523fc94c564f337ff446b8df61f91b1d64b"
)
set(
    expected_write_sha256
    "d4fe55cf0b4b975eecd160accace5b3f0e753bf05b99b2e6a5c7b7d375f256b8"
)
set(
    expected_state_write_sha256
    "6bb4604bcffd88960c62c76ae98bc5a11984a4711bef456da014b3d31cf1de4e"
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
        "02:49FC"
        --observe-hook
        "02:4A0E"
        --observe-hook
        "02:4A20"
        --observe-hook
        "02:4A32"
        --observe-hook
        "02:4A6C"
        --observe-hook
        "02:4A93"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:4A93@1"
        --work-ram-write-range
        "2CFA-2CFA"
        --work-ram-write-range
        "2D00-2D01"
        --work-ram-write-range
        "2D18-2D18"
        --work-ram-write-range
        "2D33-2D33"
        --work-ram-write-range
        "2D40-2D40"
        --work-ram-write-range
        "303E-303F"
        --work-ram-write-range
        "3042-3043"
        --work-ram-write-range
        "30BE-30BF"
        --work-ram-write-range
        "30C2-30C3"
        --work-ram-write-range
        "373E-373F"
        --work-ram-write-range
        "3742-3743"
        --work-ram-write-range
        "37BE-37BF"
        --work-ram-write-range
        "37C2-37C3"
        --work-ram-write-range
        "383E-383F"
        --work-ram-write-range
        "3842-3843"
        --work-ram-write-range
        "38BE-38BF"
        --work-ram-write-range
        "38C2-38C3"
        --work-ram-write-range
        "393E-393F"
        --work-ram-write-range
        "3942-3943"
        --work-ram-write-range
        "39BE-39BF"
        --work-ram-write-range
        "39C2-39C3"
        --work-ram-write-range
        "3A3E-3A3F"
        --work-ram-write-range
        "3A42-3A43"
        --work-ram-write-range
        "3ABE-3ABF"
        --work-ram-write-range
        "3AC2-3AC3"
        --work-ram-write-range
        "3C3E-3C3F"
        --work-ram-write-range
        "3C42-3C43"
        --work-ram-write-range
        "3CBE-3CBF"
        --work-ram-write-range
        "3CC2-3CC3"
        --work-ram-write-range
        "3D3E-3D3F"
        --work-ram-write-range
        "3D42-3D43"
        --work-ram-write-range
        "3DBE-3DBF"
        --work-ram-write-range
        "3DC2-3DC3"
        --work-ram-write-range
        "3E3E-3E3F"
        --work-ram-write-range
        "3E42-3E43"
        --work-ram-write-range
        "3EBE-3EBF"
        --work-ram-write-range
        "3EC2-3EC3"
        --work-ram-write-range
        "3F3E-3F3F"
        --work-ram-write-range
        "3F42-3F43"
        --work-ram-write-range
        "3FBE-3FBF"
        --work-ram-write-range
        "3FC2-3FC3"
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
        "Four-Star Wall-Pocket recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

set(expected_hook_counts 1 1 1 1 4 1008)
set(hook_index 1)
foreach(expected_count IN LISTS expected_hook_counts)
    set(expected_line "Observe hook ${hook_index} entries: ${expected_count}")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Four-Star Wall-Pocket output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    math(EXPR hook_index "${hook_index} + 1")
endforeach()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Routine trace complete: 1016 entries"
    "Routine call trace complete: 1016 calls, 2036 writes"
    "Routine replay 02:4A93@1 verified: frame 872"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 3951 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Four-Star Wall-Pocket output omitted '${expected_line}'.\n"
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
            "Four-Star Wall-Pocket ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Four-Star Wall-Pocket ${artifact} verified: ${actual_sha256}"
    )
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Four-Star Wall-Pocket selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Four-Star Wall-Pocket selected state verified: "
    "${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-FourStarWallPocketSemantics.ps1"
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
        "Four-Star Wall-Pocket semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Four-Star Wall-Pocket semantics verified."
    "Slots 3E/3F/42/43 map lower-left/right and upper-left/right"
    "1008 idle calls leave all four access paths inactive"
    "ROM-bounded normal access awards 10 and cycles Sorcerer color"
    "Normalized sample: 02:4A93@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Four-Star Wall-Pocket semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
