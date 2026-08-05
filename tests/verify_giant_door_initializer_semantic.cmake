# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Release")
    set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
else()
    message(FATAL_ERROR "Unsupported Giant Door initializer configuration: ${CONFIG}")
endif()

set(expected_call_sha256 "665bc72b705974fe760cb5318771f38ae9d8040c0be654adc6435fb673970e97")
set(expected_write_sha256 "83f820a0d6edaa5c0d68aa1cb249f0751a7b6d035cb8ed0b795943ea3f60c8fa")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:6294"
        --replace-hook "03:6294@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:6294@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Giant Door initializer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:6294@all entries: 1 of 1"
    "Observe hook 1 entries: 1"
    "Routine call trace complete: 1 calls, 15 writes"
    "Routine replay 03:6294@1 verified: frame 872, 345 body cycles, 15 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Giant Door initializer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
            "Semantic Giant Door initializer ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\nActual:   ${actual_sha256}"
        )
    endif()
endforeach()

message(STATUS "Semantic 6294 initializer covers the fixed Giant Door layout.")
