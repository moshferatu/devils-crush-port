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
        "Unsupported Giant-Skeleton-Eyeball gate configuration: ${CONFIG}"
    )
endif()

set(
    expected_call_sha256
    "b3c372034c15d85d66500392b79bf5e28047d0cfdb4a2766d8f2b8e9c8cd1cd2"
)
set(
    expected_write_sha256
    "dc8a4ef320c2acaa1ddf0601f0bbaaff69c83898e48da804b72e1243d9ddea4f"
)

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "02:4DC8"
        --observe-hook
        "02:4DD9"
        --replace-hook
        "02:4DC8@all"
        --replace-hook
        "02:4DD9@all"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:4DC8@1"
        --routine-replay
        "02:4DD9@1"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic Giant-Skeleton-Eyeball gate recorder failed "
        "(${recorder_result}).\n${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:4DC8@all entries: 1 of 1"
    "Native replacement 02:4DD9@all entries: 1988 of 1988"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1988"
    "Routine call trace complete: 1989 calls, 503 writes"
    "Routine replay 02:4DC8@1 verified: frame 872, 258 body cycles, 10 ordered writes"
    "Routine replay 02:4DD9@1 verified: frame 873, 57 body cycles, 0 ordered writes"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic Giant-Skeleton-Eyeball gate output omitted "
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
            "Semantic Giant-Skeleton-Eyeball gate ${artifact} artifact "
            "changed.\nExpected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic 4DC8 initializer and 4DD9 dormant gate cover all 1,989 Eyeball calls."
)
