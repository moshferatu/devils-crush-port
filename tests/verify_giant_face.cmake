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
    message(FATAL_ERROR "Unsupported Giant-Face-test configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "96aca1652644e1e8e9d67ee81b433069b80164f9350ade5a3a0b7324ec55eb6c"
)
set(
    expected_call_sha256
    "a1e9de6878930e06f0978c84a6c17acef8a184adae4252eea6d16239322f5d42"
)
set(
    expected_write_sha256
    "fc422de12e8b181f7456cb3d3d584a321be62eefeb73398bc2555ad3632f7c48"
)
set(
    expected_state_write_sha256
    "804e0ddee017e941be7cb222c24f4922fb1064b255d1d03d5fdd22c515925946"
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
        "03:7428"
        --observe-hook
        "03:742B"
        --observe-hook
        "03:742E"
        --observe-hook
        "03:74D8"
        --observe-hook
        "03:766C"
        --observe-hook
        "03:767E"
        --observe-hook
        "03:76F7"
        --observe-hook
        "03:76FA"
        --observe-hook
        "03:7718"
        --observe-hook
        "03:7726"
        --replace-hook
        "03:7428@all"
        --replace-hook
        "03:742E@all"
        --replace-hook
        "03:766C@all"
        --replace-hook
        "03:7718@all"
        --replace-hook
        "03:76F7@all"
        --replace-hook
        "03:74D8@all"
        --replace-hook
        "03:767E@all"
        --replace-hook
        "03:76FA@all"
        --replace-hook
        "03:7726@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:7428@1"
        --routine-replay
        "03:742B@1"
        --routine-replay
        "03:742E@1"
        --routine-replay
        "03:766C@1"
        --routine-replay
        "03:7718@1"
        --routine-replay
        "03:76F7@1"
        --routine-replay
        "03:74D8@1"
        --routine-replay
        "03:767E@1"
        --routine-replay
        "03:76FA@1"
        --routine-replay
        "03:7726@1"
        --work-ram-write-range
        "2258-2258"
        --work-ram-write-range
        "2D0C-2D0E"
        --work-ram-write-range
        "2D21-2D21"
        --work-ram-write-range
        "3036-3039"
        --work-ram-write-range
        "3836-3839"
        --work-ram-write-range
        "3A36-3A39"
        --work-ram-write-range
        "3AB6-3AB9"
        --work-ram-write-range
        "3E36-3E39"
        --work-ram-write-range
        "3EB6-3EB9"
        --work-ram-write-range
        "3F36-3F39"
        --work-ram-write-range
        "3FB6-3FB9"
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
        "Giant-Face recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

set(expected_hook_counts 1 1 1 2648 1 873 1 867 1 1214)
set(hook_index 1)
foreach(expected_count IN LISTS expected_hook_counts)
    set(expected_line "Observe hook ${hook_index} entries: ${expected_count}")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant-Face output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    math(EXPR hook_index "${hook_index} + 1")
endforeach()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:7428@all entries: 1 of 1"
    "Native replacement 03:742E@all entries: 1 of 1"
    "Native replacement 03:766C@all entries: 1 of 1"
    "Native replacement 03:7718@all entries: 1 of 1"
    "Native replacement 03:76F7@all entries: 1 of 1"
    "Native replacement 03:74D8@all entries: 2648 of 2648"
    "Native replacement 03:767E@all entries: 873 of 873"
    "Native replacement 03:76FA@all entries: 867 of 867"
    "Native replacement 03:7726@all entries: 1214 of 1214"
    "Routine trace complete: 5608 entries"
    "Routine call trace complete: 5608 calls, 812 writes"
    "Routine replay 03:7428@1 verified: frame 872, 192 body cycles, 7 ordered writes"
    "Routine replay 03:742B@1 verified: frame 873, 192 body cycles, 7 ordered writes"
    "Routine replay 03:742E@1 verified: frame 941, 306 body cycles, 12 ordered writes"
    "Routine replay 03:766C@1 verified: frame 872, 255 body cycles, 10 ordered writes"
    "Routine replay 03:7718@1 verified: frame 872, 243 body cycles, 9 ordered writes"
    "Routine replay 03:76F7@1 verified: frame 872, 192 body cycles, 7 ordered writes"
    "Routine replay 03:74D8@1 verified: frame 942"
    "Routine replay 03:767E@1 verified: frame 1222"
    "Routine replay 03:76FA@1 verified: frame 1223"
    "Routine replay 03:7726@1 verified: frame 1213"
    "Routine replay complete: 10 state-normalized calls"
    "Work-RAM write trace complete: 127 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant-Face output omitted '${expected_line}'.\n"
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
            "Giant-Face ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Giant-Face ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Giant-Face selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Giant-Face selected state verified: ${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-GiantFaceSemantics.ps1"
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
        "Giant-Face semantics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Giant-Face semantics verified."
    "Fixed slots 36-39 map the form controller"
    "5608 routed calls retain inactive form 0"
    "ROM-bounded rule: request bits wake and advance six transformations"
    "ROM-bounded collisions: crown/closed mouth award 3"
    "Normalized samples: 03:74D8@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant-Face semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
