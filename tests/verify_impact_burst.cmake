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
    message(FATAL_ERROR "Unsupported impact-burst-test configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "d2741f1bda0e28a64ad1b951f2542a30ffac60b51a4a69ebf694724a0f192f51"
)
set(
    expected_call_sha256
    "db3caaa99f3cf5704fecbb2add21ee110c14b809555dc0c83bd8e3b6f7adffcf"
)
set(
    expected_write_sha256
    "0ddbb0da9a4af51466a80a7258dcf88500a966d6b2c92404fe7ddbff06241911"
)
set(
    expected_actor_write_sha256
    "b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338"
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
        "02:41EC"
        --observe-hook
        "02:41FF"
        --observe-hook
        "02:4290"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:4290@1"
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
        "Impact-burst recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 5"
    "Observe hook 2 entries: 2"
    "Observe hook 3 entries: 7"
    "Routine trace complete: 14 entries"
    "Routine call trace complete: 14 calls, 1231 writes"
    "Routine replay 02:4290@1 verified: frame 1852"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 3872 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Impact-burst output omitted '${expected_line}'.\n"
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
            "Impact-burst ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Impact-burst ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${actor_write_output}" actual_actor_write_sha256)
if(NOT actual_actor_write_sha256 STREQUAL expected_actor_write_sha256)
    message(
        FATAL_ERROR
        "Impact-burst actor-identity artifact changed.\n"
        "Expected: ${expected_actor_write_sha256}\n"
        "Actual:   ${actual_actor_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Impact-burst actor identity verified: ${actual_actor_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-ImpactBurstSemantics.ps1"
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
        "Impact-burst semantics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Impact-burst semantics verified."
    "Shared sources: Small Soldiers x2, Tower insects x3"
    "Seven type-8F effects"
    "Source-selected feedback: command 2D x5, 34 x1, 39 x1"
    "First normalized sample: 02:4290@1, frame 1852"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Impact-burst semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")

# The lifecycle controller is verified separately from the wider allocator /
# activation oracle above. Its seven terminal calls tail-jump through C027 into
# the generic actor reset, so the direct call trace also proves nested native
# composition across that boundary.
set(lifetime_output "${OUTPUT}.lifetime-native.csv")
set(lifetime_call_output "${lifetime_output}.calls.csv")
set(lifetime_write_output "${lifetime_call_output}.writes.csv")
set(
    expected_lifetime_call_sha256
    "02d0567d62b5130c74a6a310762badcb9b882bf03abe5d4983edd0415ee0f06d"
)
set(
    expected_lifetime_write_sha256
    "93774afdee74412026678a8e86f9cb969fc18ab8bf7924477ab25c3255d6dcae"
)

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${lifetime_output}"
        --replace-hook
        "02:42B0@all"
        --observe-hook
        "02:42B0"
        --routine-call-trace
        "${lifetime_call_output}"
        --routine-replay
        "02:42B0@1"
        --rom
        "${ROM}"
    RESULT_VARIABLE lifetime_result
    OUTPUT_VARIABLE lifetime_stdout
    ERROR_VARIABLE lifetime_stderr
)
if(NOT lifetime_result EQUAL 0)
    message(
        FATAL_ERROR
        "Native impact-burst lifetime recorder failed (${lifetime_result}).\n"
        "${lifetime_stdout}${lifetime_stderr}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:42B0@all entries: 252 of 252"
    "Observe hook 1 entries: 252"
    "Routine call trace complete: 252 calls, 1502 writes"
    "Routine replay 02:42B0@1 verified: frame 1852, 60 body cycles, 1 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${lifetime_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Native impact-burst output omitted '${expected_line}'.\n"
            "${lifetime_stdout}${lifetime_stderr}"
        )
    endif()
endforeach()

file(SHA256 "${lifetime_output}" actual_lifetime_census_sha256)
if(NOT actual_lifetime_census_sha256 STREQUAL expected_census_sha256)
    message(
        FATAL_ERROR
        "Native impact-burst census changed.\n"
        "Expected: ${expected_census_sha256}\n"
        "Actual:   ${actual_lifetime_census_sha256}\n"
    )
endif()
file(SHA256 "${lifetime_call_output}" actual_lifetime_call_sha256)
if(NOT actual_lifetime_call_sha256 STREQUAL expected_lifetime_call_sha256)
    message(
        FATAL_ERROR
        "Native impact-burst call trace changed.\n"
        "Expected: ${expected_lifetime_call_sha256}\n"
        "Actual:   ${actual_lifetime_call_sha256}\n"
    )
endif()
file(SHA256 "${lifetime_write_output}" actual_lifetime_write_sha256)
if(NOT actual_lifetime_write_sha256 STREQUAL expected_lifetime_write_sha256)
    message(
        FATAL_ERROR
        "Native impact-burst write trace changed.\n"
        "Expected: ${expected_lifetime_write_sha256}\n"
        "Actual:   ${actual_lifetime_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Native impact-burst lifetime verified: 252 calls, 1,502 writes."
)
