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
        "Unsupported Big-Star-feature-network configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "09eab95b760dce5e1113ff412a35f4e554d1f8c83008dd2b8688f5634af6d3e6"
)
set(
    expected_call_sha256
    "7677de9c364d382d657efbea5d2272322f50ba42c8253948efe6bb5b3acd6f36"
)
set(
    expected_write_sha256
    "4be8e9c06b82726a381b316d6eee5f7f5a6b725e75b38c7770e0c17f4c432ce0"
)
set(
    expected_state_sha256
    "99f8a2f2ba146a08abc02fb2ebcc52f1f5fe0504636628cca249c95616776627"
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
        "02:4455"
        --observe-hook
        "02:445B"
        --observe-hook
        "02:4485"
        --observe-hook
        "02:44E4"
        --observe-hook
        "02:565C"
        --observe-hook
        "03:61A0"
        --observe-hook
        "02:5C4B"
        --observe-hook
        "02:5C7C"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:445B@1"
        --work-ram-write-range
        "2CF5-2CFA"
        --work-ram-write-range
        "2D00-2D05"
        --work-ram-write-range
        "2D0F-2D0F"
        --work-ram-write-range
        "2D18-2D18"
        --work-ram-write-range
        "3044-304F"
        --work-ram-write-range
        "30C4-30CF"
        --work-ram-write-range
        "3344-334F"
        --work-ram-write-range
        "33C4-33CF"
        --work-ram-write-range
        "3744-374F"
        --work-ram-write-range
        "37C4-37CF"
        --work-ram-write-range
        "3844-384F"
        --work-ram-write-range
        "38C4-38CF"
        --work-ram-write-range
        "3944-394F"
        --work-ram-write-range
        "39C4-39CF"
        --work-ram-write-range
        "3A44-3A4F"
        --work-ram-write-range
        "3AC4-3ACF"
        --work-ram-write-range
        "3B44-3B4F"
        --work-ram-write-range
        "3BC4-3BCF"
        --work-ram-write-range
        "3C44-3C4F"
        --work-ram-write-range
        "3CC4-3CCF"
        --work-ram-write-range
        "3E44-3E4F"
        --work-ram-write-range
        "3EC4-3ECF"
        --work-ram-write-range
        "3F44-3F4F"
        --work-ram-write-range
        "3FC4-3FCF"
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
        "Big-Star feature-network recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 2650"
    "Observe hook 3 entries: 2"
    "Observe hook 4 entries: 4"
    "Observe hook 5 entries: 1"
    "Observe hook 6 entries: 1"
    "Observe hook 7 entries: 1"
    "Observe hook 8 entries: 1"
    "Routine trace complete: 2661 entries"
    "Routine call trace complete: 2661 calls, 487 writes"
    "Routine replay 02:445B@1 verified: frame 873, 21 body cycles"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 7106 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Big-Star feature-network output omitted '${expected_line}'.\n"
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
            "Big-Star feature-network ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Big-Star feature-network ${artifact} verified: ${actual_sha256}"
    )
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BigStarFeatureNetworkSemantics.ps1"
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
        "Big-Star feature-network analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Big-Star feature-network semantics verified."
    "Fixed family: Little-Skull pairs 46/47 and 49/4A"
    "Selector registry: I=Undead Head"
    "Inactive Little-Skull hit: 14 stored units"
    "Big-Star speeds: +64,+32,+16,0,-16,-32,-64"
    "Routed coverage: 11 initializers and 2,650 clear-latch calls"
    "First normalized sample: 02:445B@1, frame 873, 21 cycles"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Big-Star feature-network semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
