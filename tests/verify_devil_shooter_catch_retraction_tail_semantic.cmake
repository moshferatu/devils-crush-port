# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(
        FATAL_ERROR
        "Unsupported catch-retraction-tail configuration: ${CONFIG}"
    )
endif()

set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_call_sha256
    "cbfa183a7196791a216c7da5faaacfdda5a05e80a6933308f8f595a9de6e1aaa"
)
set(
    expected_write_sha256
    "959a19129762f206f6a8b31f7191ebb28776e09f04e5ff635e0ca7d2999c3e50"
)

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(instruction_profile_output "${OUTPUT}.profile.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "02:52C6"
        --replace-hook
        "02:52C6@all"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:52C6@1"
        --instruction-profile
        "${instruction_profile_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic Devil-Shooter catch-retraction-tail recorder failed "
        "(${recorder_result}).\n${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:52C6@all entries: 20 of 20"
    "Observe hook 1 entries: 20"
    "Routine call trace complete: 20 calls, 119 writes"
    "Routine replay 02:52C6@1 verified: frame 1065, 105 body cycles, 2 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic Devil-Shooter catch-retraction-tail output omitted "
            "'${expected_line}'.\n${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
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
            "Semantic Devil-Shooter catch-retraction-tail ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

# The 19 steady animation calls own their terminal RTS. The observer artifact
# above still ends each call at 02:52E2; this proves the processor did not also
# execute that return after the transcript.
file(READ "${instruction_profile_output}" instruction_profile)
string(FIND "${instruction_profile}" "02,52E2," return_tail_offset)
if(NOT return_tail_offset EQUAL -1)
    message(
        FATAL_ERROR
        "Devil-Shooter catch-retraction RTS 02:52E2 remains emulated."
    )
endif()

message(
    STATUS
    "Semantic 52C6 tail preserves all 20 catch-retraction continuations."
)
