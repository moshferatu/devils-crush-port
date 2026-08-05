# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(expected_census_sha256 "a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2")
elseif(CONFIG STREQUAL "Release")
    set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
else()
    message(FATAL_ERROR "Unsupported Gray-Blob initializer configuration: ${CONFIG}")
endif()

set(expected_call_sha256 "d8d4381418800d61f2e06b4d458b1f3a9b37d8dc74081e1d6866703c700420e8")
set(expected_write_sha256 "9b5c4d13a3bd3fcaa229ef2b23fa0094c7fa57af9ec5eb898ace6e19ded38193")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:6876"
        --replace-hook "03:6876@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:6876@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Gray-Blob initializer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:6876@all entries: 7 of 7"
    "Observe hook 1 entries: 7"
    "Routine call trace complete: 7 calls, 105 writes"
    "Routine replay 03:6876@1 verified: frame 872, 372 body cycles, 15 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Gray-Blob initializer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
            "Semantic Gray-Blob initializer ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\nActual:   ${actual_sha256}"
        )
    endif()
endforeach()

message(STATUS "Semantic 6876 initializer covers all seven Gray-Blob layouts.")
