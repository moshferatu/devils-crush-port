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
        "Unsupported Walled-Star-access configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "bef6b62849397616eabdbc4f63247da80689c1472925c28e4bbc438ba82f830b"
)
set(
    expected_call_sha256
    "1ff4a18a3f7caa572e46000abcd46598b96f3239fae0e8c75cec3a7a98aad3b8"
)
set(
    expected_write_sha256
    "26c396dc657674d9d4e93c3ea07cd76390ef2ae9bb12bb7878997ede8bc4a8a5"
)
set(
    expected_state_write_sha256
    "dcaf467b0f5e7bc35b3051723ddebb6da2ef5f4266c8573d15f48f3b34d6c876"
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
        "02:4A44"
        --observe-hook
        "02:4B3F"
        --replace-hook
        "02:4B3F@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:4B3F@1"
        --work-ram-write-range
        "2053-2055"
        --work-ram-write-range
        "2D32-2D32"
        --work-ram-write-range
        "2D53-2D53"
        --work-ram-write-range
        "2DBC-2DBC"
        --work-ram-write-range
        "3011-3011"
        --work-ram-write-range
        "3091-3091"
        --work-ram-write-range
        "3811-3811"
        --work-ram-write-range
        "3891-3891"
        --work-ram-write-range
        "3911-3911"
        --work-ram-write-range
        "3991-3991"
        --work-ram-write-range
        "3A11-3A11"
        --work-ram-write-range
        "3A91-3A91"
        --work-ram-write-range
        "3C11-3C11"
        --work-ram-write-range
        "3C91-3C91"
        --work-ram-write-range
        "3D11-3D11"
        --work-ram-write-range
        "3D91-3D91"
        --work-ram-write-range
        "3E11-3E11"
        --work-ram-write-range
        "3E91-3E91"
        --work-ram-write-range
        "3F11-3F11"
        --work-ram-write-range
        "3F91-3F91"
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
        "3ADB-3ADB"
        --work-ram-write-range
        "3D5B-3D5B"
        --work-ram-write-range
        "3DDB-3DDB"
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
        "Walled-Star-access recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 905"
    "Native replacement 02:4B3F@all entries: 905 of 905"
    "Routine trace complete: 906 entries"
    "Routine call trace complete: 906 calls, 1800 writes"
    "Routine replay 02:4B3F@1 verified: frame 871, 135 body cycles, 3 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 2790 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Walled-Star-access output omitted '${expected_line}'.\n"
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
            "Walled-Star-access ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Walled-Star-access ${artifact} verified: ${actual_sha256}"
    )
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Walled-Star-access selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Walled-Star-access selected state verified: "
    "${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-WalledStarAccessSemantics.ps1"
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
        "Walled-Star-access semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Walled-Star access semantics verified."
    "Fixed slot 11 at (38,308)"
    "905 live calls remain unhit"
    "selects a random bonus stage from 1 through 6"
    "Normalized sample: 02:4B3F@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Walled-Star-access semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
