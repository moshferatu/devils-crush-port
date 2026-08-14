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
    message(FATAL_ERROR "Unsupported Spider-appearance configuration: ${CONFIG}")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(expected_call_sha256 "5621b48b8cd7042c1ec1a7fac86efa12a561f016f5d0598af05e0105eef50b3c")
set(expected_write_sha256 "d7c55c4dde46c9c844669f67e5a3e932d411fa8aac206d6e1d9bc284fccac210")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:65A4"
        --replace-hook "03:65A4@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:65A4@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Spider-appearance recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:65A4@all entries: 5441 of 5441"
    "Observe hook 1 entries: 5441"
    "Routine replay 03:65A4@1 verified: frame 873, 102 body cycles, 3 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Spider-appearance output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

file(SHA256 "${OUTPUT}" actual_census_sha256)
if(NOT actual_census_sha256 STREQUAL expected_census_sha256)
    message(FATAL_ERROR "Semantic Spider-appearance census changed.\nExpected: ${expected_census_sha256}\nActual:   ${actual_census_sha256}\n")
endif()

foreach(artifact IN ITEMS call write)
    if(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Semantic Spider-appearance ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 65A4 shared Spider appearance helper covers all 5,441 calls with checkpointed interrupts.")
