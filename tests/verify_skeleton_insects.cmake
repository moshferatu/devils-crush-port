# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
    )
else()
    message(FATAL_ERROR "Unsupported Skeleton-Insect-test configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "459cf376c5bc819e46183e47928813e0e19ab663c7cd5166449c3a5c394c2f8b"
)
set(
    expected_call_sha256
    "5bf902a75085dd39b72fd60ffdd182c89e78bcde852a333809caeb8ab0f3a348"
)
set(
    expected_write_sha256
    "024c0aebd843222cfea420e4144b2134004d64755927cb21ceddf27dbbd78f86"
)
set(
    expected_actor_write_sha256
    "c61ab3a4f0c35687c64ff7991f77e12ecf2864d8bb06ec8318a901db3c252d0a"
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
        "02:54A3"
        --observe-hook
        "02:54B9"
        --observe-hook
        "02:54D0"
        --observe-hook
        "02:5500"
        --observe-hook
        "02:5543"
        --observe-hook
        "02:5551"
        --observe-hook
        "02:5568"
        --observe-hook
        "02:5590"
        --replace-hook
        "02:5590@all"
        --observe-hook
        "02:55D3"
        --observe-hook
        "02:55D6"
        --observe-hook
        "02:55D9"
        --observe-hook
        "02:55F9"
        --observe-hook
        "02:561A"
        --replace-hook
        "02:561A@all"
        --observe-hook
        "02:5625"
        --observe-hook
        "02:5630"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:54B9@1"
        --routine-replay
        "02:5551@1"
        --routine-replay
        "02:5590@1"
        --routine-replay
        "02:55F9@1"
        --routine-replay
        "02:561A@1"
        --work-ram-write-range
        "2CFC-2CFC"
        --work-ram-write-range
        "2D13-2D14"
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
        "Skeleton-Insect recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

set(expected_hook_counts 1 1631 12 80 1 336 8 1084 1 1 1 2403 16 1 229)
set(hook_index 1)
foreach(expected_count IN LISTS expected_hook_counts)
    set(expected_line "Observe hook ${hook_index} entries: ${expected_count}")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Skeleton-Insect output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    math(EXPR hook_index "${hook_index} + 1")
endforeach()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:5590@all entries: 1084 of 1084"
    "Native replacement 02:561A@all entries: 16 of 16"
    "Routine trace complete: 5805 entries"
    "Routine call trace complete: 5805 calls, 1316 writes"
    "Routine replay 02:54B9@1 verified: frame 873"
    "Routine replay 02:5551@1 verified: frame 873"
    "Routine replay 02:5590@1 verified: frame 2191, 54 body cycles, 0 ordered writes"
    "Routine replay 02:55F9@1 verified: frame 942"
    "Routine replay 02:561A@1 verified: frame 3353, 171 body cycles, 6 ordered writes"
    "Routine replay complete: 5 state-normalized calls"
    "Work-RAM write trace complete: 3882 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Skeleton-Insect output omitted '${expected_line}'.\n"
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
            "Skeleton-Insect ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Skeleton-Insect ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${actor_write_output}" actual_actor_write_sha256)
if(NOT actual_actor_write_sha256 STREQUAL expected_actor_write_sha256)
    message(
        FATAL_ERROR
        "Skeleton-Insect selected-state artifact changed.\n"
        "Expected: ${expected_actor_write_sha256}\n"
        "Actual:   ${actual_actor_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Skeleton-Insect selected state verified: ${actual_actor_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-SkeletonInsectSemantics.ps1"
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
        "Skeleton-Insect semantics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Skeleton-Insect semantics verified."
    "Three fixed targets: left slot 31, center slot 32, right slot 33."
    "Front-Spider latches 2D13/2D14 trigger +40 stored units"
    "Tower latch 2CFC triggers +100 stored units"
    "The three target scopes add no audio command"
    "Normalized idle samples: 02:54B9@1 and 02:5551@1"
    "Native right post-trigger sample: 02:5590@1, frame 2191"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Skeleton-Insect semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
