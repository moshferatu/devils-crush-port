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
    message(FATAL_ERROR "Unsupported Sorcerer-test configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "d181d3e55ed964c6dea753855f1d93f63bed9a4a16b9192e10cce080a631ff8c"
)
set(
    expected_call_sha256
    "c7e96960d04699e62f06ba3a7773b8e72d49b2b7e473e09529b131a34fd8b865"
)
set(
    expected_write_sha256
    "8ef36479191f192a24b5fc3eb79fde344469c02ab99d0ccd49d535d68e09ac99"
)
set(
    expected_actor_write_sha256
    "5060cead3f36f5e807e1804fcba4b2675455a9d259a8ac455e45f8e814972407"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(actor_write_output "${OUTPUT}.actors.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "02:578E"
        --observe-hook
        "02:57A4"
        --observe-hook
        "02:57A7"
        --observe-hook
        "02:57AD"
        --observe-hook
        "02:57F8"
        --observe-hook
        "02:5808"
        --observe-hook
        "02:586F"
        --observe-hook
        "02:5886"
        --observe-hook
        "02:58C2"
        --replace-hook
        "02:57F8@all"
        --replace-hook
        "02:5808@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:57AD@1"
        --routine-replay
        "02:57F8@1"
        --routine-replay
        "02:5808@1"
        --routine-replay
        "02:5886@1"
        --routine-replay
        "02:58C2@1"
        --work-ram-write-range
        "2D0B-2D10"
        --work-ram-write-range
        "2D40-2D40"
        --work-ram-write-range
        "3000-307F"
        --work-ram-write-range
        "3F00-3FFF"
        --work-ram-write-trace
        "${actor_write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Sorcerer recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

set(expected_hook_counts 1 1 1 2648 8 8 1240 1800 18152)
set(hook_index 1)
foreach(expected_count IN LISTS expected_hook_counts)
    set(expected_line "Observe hook ${hook_index} entries: ${expected_count}")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Sorcerer output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    math(EXPR hook_index "${hook_index} + 1")
endforeach()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:57F8@all entries: 8 of 8"
    "Native replacement 02:5808@all entries: 8 of 8"
    "Routine trace complete: 23859 entries"
    "Routine call trace complete: 23859 calls, 346654 writes"
    "Routine replay 02:57AD@1 verified: frame 942"
    "Routine replay 02:57F8@1 verified: frame 871, 249 body cycles, 10 ordered writes"
    "Routine replay 02:5808@1 verified: frame 873, 642 body cycles, 22 ordered writes"
    "Routine replay 02:5886@1 verified: frame 942"
    "Routine replay 02:58C2@1 verified: frame 1167"
    "Routine replay complete: 5 state-normalized calls"
    "Work-RAM write trace complete: 3904 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Sorcerer output omitted '${expected_line}'.\n"
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
            "Sorcerer ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Sorcerer ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${actor_write_output}" actual_actor_write_sha256)
if(NOT actual_actor_write_sha256 STREQUAL expected_actor_write_sha256)
    message(
        FATAL_ERROR
        "Sorcerer selected-state artifact changed.\n"
        "Expected: ${expected_actor_write_sha256}\n"
        "Actual:   ${actual_actor_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Sorcerer selected state verified: ${actual_actor_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-SorcererSemantics.ps1"
        -RomPath
        "${ROM}"
        -CallPath
        "${call_output}"
        -WritePath
        "${write_output}"
        -ActorWritePath
        "${actor_write_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Sorcerer semantics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Sorcerer semantics verified."
    "Controller slot 16 deploys fixed Sorcerer slots 17-1E"
    "All eight reach orbit dispatch 02:58C2"
    "ROM-bounded inactive rule: two 10-unit hits, command 29"
    "Normalized samples: controller 02:57AD@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Sorcerer semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
