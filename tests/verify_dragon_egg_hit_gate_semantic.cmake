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
        "Unsupported Dragon-Egg-hit-gate configuration: ${CONFIG}"
    )
endif()

set(
    expected_call_sha256
    "660ca6254317cfc5d58251dbba945c8af6ea0dcc616baf07ddc118e7bf2b6337"
)
set(
    expected_write_sha256
    "3fb20b0f8a9359c995f83c83f1bb0e96e1aa2c1c88158dbc162017d063a3ffd0"
)

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "02:4345"
        --replace-hook
        "02:4345@all"
        --replace-hook
        "02:4357@all"
        --replace-hook
        "02:4371@all"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:4345@1"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic Dragon-Egg hit-gate recorder failed "
        "(${recorder_result}).\n${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:4345@all entries: 9857 of 9857"
    "Native replacement 02:4357@all entries: 1 of 1"
    "Native replacement 02:4371@all entries: 1 of 1"
    "Observe hook 1 entries: 9857"
    "Routine call trace complete: 9857 calls, 1682 writes"
    "Routine replay 02:4345@1 verified: frame 873, 21 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic Dragon-Egg hit-gate output omitted "
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
            "Semantic Dragon-Egg hit-gate ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic 4345 gate covers all 9,857 calls; 4357/4371 own routed setup."
)
