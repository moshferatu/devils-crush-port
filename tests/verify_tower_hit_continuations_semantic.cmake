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
        "Unsupported Tower-hit-continuation configuration: ${CONFIG}"
    )
endif()

set(
    expected_call_sha256
    "7e282adac47c3b913dc254b24cb364e41a12ac7c216f5d9944bac4a1e38306b8"
)
set(
    expected_write_sha256
    "eb7ffe66312e2c613cb842a6e755f8facba6574218e069a93be809ec2d27ba04"
)

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "03:60E0"
        --observe-hook
        "03:611C"
        --replace-hook
        "03:60C3@all"
        --replace-hook
        "03:60E0@all"
        --replace-hook
        "03:611C@all"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:611C@1"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic Tower-hit-continuation recorder failed "
        "(${recorder_result}).\n${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:60C3@all entries: 2646 of 2646"
    "Native replacement 03:60E0@all entries: 1 of 1"
    "Native replacement 03:611C@all entries: 4 of 4"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 4"
    "Routine call trace complete: 5 calls, 54 writes"
    "Routine replay 03:611C@1 verified: frame 3353, 135 body cycles, 5 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic Tower-hit-continuation output omitted "
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
            "Semantic Tower-hit-continuation ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic 60E0/611C continuations preserve the shared dispatcher interrupt."
)
