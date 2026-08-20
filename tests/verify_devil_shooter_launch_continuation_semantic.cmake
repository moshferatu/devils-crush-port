# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(
        FATAL_ERROR
        "Unsupported launch-continuation configuration: ${CONFIG}"
    )
endif()

set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_call_sha256
    "48b37d777b5c4f1c76ba3bdd9f725a973727f677e93ab8dcf9f93453076d3c99"
)
set(
    expected_write_sha256
    "cbd4200c162c161e5232ef6676286fe2e59303a8d5a18047f3b543d20ee5efe9"
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
        "02:520B"
        --replace-hook
        "02:520B@all"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:520B@5"
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
        "Semantic Devil-Shooter launch-continuation recorder failed "
        "(${recorder_result}).\n${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:520B@all entries: 12 of 12"
    "Observe hook 1 entries: 12"
    "Routine call trace complete: 12 calls, 794 writes"
    "Routine replay 02:520B@5 verified: frame 1190, 2913 body cycles, 61 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic Devil-Shooter launch-continuation output omitted "
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
            "Semantic Devil-Shooter launch-continuation ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

# Eleven active-charge calls jump directly to the native appearance submitter.
# The final zero-charge call intentionally resumes at $5216 for cleanup.
file(READ "${instruction_profile_output}" instruction_profile)
foreach(removed_pc IN ITEMS 5210 5213)
    string(FIND "${instruction_profile}" "02,${removed_pc}," removed_offset)
    if(NOT removed_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Devil-Shooter launch instruction 02:${removed_pc} remains emulated."
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic 520B continuation preserves all 12 launch decisions."
)
