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
    message(FATAL_ERROR "Unsupported Knight waiting-parent configuration: ${CONFIG}")
endif()
set(expected_call_sha256 "64190c02108f2a3b23ffb5ab32068d5e54879b865f70946c5ea9258ff76d1eba")
set(expected_write_sha256 "66e905370e6333ef4466eb922b24f1ae04897f1599ea77582e03f6c58e4c36e9")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:7378"
        --replace-hook "03:7378@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:7378@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Knight-waiting-parent recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:7378@all entries: 10600 of 10600"
    "Observe hook 1 entries: 10600"
    "Routine call trace complete: 10600 calls, 27532 writes"
    "Routine replay 03:7378@1 verified: frame 873, 123 body cycles, 2 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Knight-waiting-parent output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Knight-waiting-parent ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 7378 queued-Knight waiting parent composes the idle child across all 10,600 calls.")
