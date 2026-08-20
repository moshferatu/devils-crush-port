# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported Skeleton-Insect-countdown configuration: ${CONFIG}")
endif()

set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_call_sha256
    "4967774d010215354a92ed7dc47f69707feebc3067d6c2c847e7b00912598dc2"
)
set(
    expected_write_sha256
    "bd022874aab2d8e8256f76de0e9d78cc580cb04405c47c38d8b9e7f6723cc41e"
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
        "02:54D0"
        --replace-hook
        "02:54D0@all"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:54D0@4"
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
        "Semantic Skeleton-Insect startup-countdown recorder failed "
        "(${recorder_result}).\n${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:54D0@all entries: 12 of 12"
    "Observe hook 1 entries: 12"
    "Routine call trace complete: 12 calls, 44 writes"
    "Routine replay 02:54D0@4 verified: frame 3470, 132 body cycles, 4 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic Skeleton-Insect startup-countdown output omitted "
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
            "Semantic Skeleton-Insect startup-countdown ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

file(READ "${instruction_profile_output}" instruction_profile)
string(FIND "${instruction_profile}" "02,54D0,12,567," native_entry_offset)
if(native_entry_offset EQUAL -1)
    message(FATAL_ERROR "Skeleton-Insect countdown entry lacks native cycle credit.")
endif()
string(FIND "${instruction_profile}" "02,54D3," removed_branch_offset)
if(NOT removed_branch_offset EQUAL -1)
    message(FATAL_ERROR "Skeleton-Insect countdown branch 02:54D3 remains emulated.")
endif()
string(FIND "${instruction_profile}" "02,54EC,2,42," remaining_return_offset)
if(remaining_return_offset EQUAL -1)
    message(FATAL_ERROR "Skeleton-Insect countdown transition return profile changed.")
endif()

message(STATUS "Semantic 54D0 countdown preserves all 12 startup decisions.")
