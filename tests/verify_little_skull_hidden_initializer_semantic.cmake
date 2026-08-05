# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Little-Skull hidden initializer configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "80ab3f8eddb9873faa352caaad825dad72ee7a85330246e4644046e0e5256d50")
set(expected_write_sha256 "47f595cd8ce7b2e60f21df5a854742de21b64aeca3f1d6be16a4bf86dcc4f980")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "02:4455"
        --replace-hook "02:4455@all"
        --routine-call-trace "${call_output}"
        --routine-replay "02:4455@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Little-Skull hidden initializer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:4455@all entries: 1 of 1"
    "Observe hook 1 entries: 1"
    "Routine call trace complete: 1 calls, 8 writes"
    "Routine replay 02:4455@1 verified: frame 872, 207 body cycles, 8 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Little-Skull hidden initializer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()
foreach(artifact IN ITEMS census call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
        set(expected_sha256 "${expected_census_sha256}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
        set(expected_sha256 "${expected_call_sha256}")
    else()
        set(artifact_path "${write_output}")
        set(expected_sha256 "${expected_write_sha256}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Semantic Little-Skull hidden initializer ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()
message(STATUS "Semantic 4455 initializer covers the fixed Little-Skull hidden controller.")
