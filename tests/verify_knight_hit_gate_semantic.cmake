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
    message(FATAL_ERROR "Unsupported Knight-hit-gate configuration: ${CONFIG}")
endif()

set(expected_call_sha256 "8ed50f2eaca81ef769e8e7eed1283965323f40e91d8abd9b4753dd7e64c24917")
set(expected_write_sha256 "9d7282f06841f7e4dcbd40d6ccfeb507647c85afe351251be26d2c04832f31da")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:73D8"
        --replace-hook "03:73D8@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:73D8@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Knight-hit-gate recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:73D8@all entries: 10600 of 10600"
    "Observe hook 1 entries: 10600"
    "Routine call trace complete: 10600 calls, 1539 writes"
    "Routine replay 03:73D8@1 verified: frame 873, 33 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Knight-hit-gate output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Knight-hit-gate ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 73D8 queued-Knight idle gate covers all 10,600 calls with checkpointed interrupts.")
