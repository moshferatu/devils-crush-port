# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported right Skeleton-Insect-countdown configuration: ${CONFIG}")
endif()

set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_call_sha256
    "b958c25a1d1950616223a0c52d9310faa23ed6fbf2e774fe2086a4e933b0d2c0"
)
set(
    expected_write_sha256
    "5b8c6dd68f3a53567abfd0999f9ff6b936de507854be7cd5682b4714d68e306b"
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
        "02:5568"
        --replace-hook
        "02:5568@all"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:5568@4"
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
        "Semantic right Skeleton-Insect startup-countdown recorder failed "
        "(${recorder_result}).\n${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:5568@all entries: 8 of 8"
    "Observe hook 1 entries: 8"
    "Routine call trace complete: 8 calls, 22 writes"
    "Routine replay 02:5568@4 verified: frame 2186, 132 body cycles, 4 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic right Skeleton-Insect startup-countdown output omitted "
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
            "Semantic right Skeleton-Insect startup-countdown ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

file(READ "${instruction_profile_output}" instruction_profile)
string(FIND "${instruction_profile}" "02,5568,8,378," native_entry_offset)
if(native_entry_offset EQUAL -1)
    message(FATAL_ERROR "Right Skeleton-Insect countdown entry lacks native cycle credit.")
endif()
string(FIND "${instruction_profile}" "02,556B," removed_branch_offset)
if(NOT removed_branch_offset EQUAL -1)
    message(FATAL_ERROR "Right Skeleton-Insect countdown branch 02:556B remains emulated.")
endif()
string(FIND "${instruction_profile}" "02,5584,1,21," remaining_return_offset)
if(remaining_return_offset EQUAL -1)
    message(FATAL_ERROR "Right Skeleton-Insect countdown transition return profile changed.")
endif()

message(STATUS "Semantic 5568 countdown preserves all 8 startup decisions.")
