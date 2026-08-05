# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Gray-Blob first-live configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "97f5089683bb67c53a3755a2d320ce1bcf5ea5e347d65200373a3f22b69cbc4b")
set(expected_call_sha256 "01eef0061275b2850f5d2c175b2773a25df577e1ead03a9b768aaa6e59155a4c")
set(expected_write_sha256 "47a7aeb1a16655d15c7e68e97753555d4354f029a6c9cebd738d584681be66b0")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-gameplay "${OUTPUT}"
        --work-ram-patch "1300:2CC0:80"
        --observe-hook "03:68A1"
        --replace-hook "03:68A1@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:68A1@1"
        --rom "${ROM}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Semantic Gray-Blob first-live recorder failed (${result}).\n${out}${err}")
endif()
foreach(line IN ITEMS
    "Native replacement 03:68A1@all entries: 7 of 7"
    "Observe hook 1 entries: 7"
    "Routine call trace complete: 7 calls, 140 writes"
    "Routine replay 03:68A1@1 verified: frame 1541, 633 body cycles, 20 ordered writes"
)
    string(FIND "${out}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Semantic Gray-Blob first-live output omitted '${line}'.\n${out}${err}")
    endif()
endforeach()
foreach(kind IN ITEMS census call write)
    if(kind STREQUAL "census")
        set(path "${OUTPUT}")
    elseif(kind STREQUAL "call")
        set(path "${call_output}")
    else()
        set(path "${write_output}")
    endif()
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL "${expected_${kind}_sha256}")
        message(FATAL_ERROR "Semantic Gray-Blob first-live ${kind} artifact changed.")
    endif()
endforeach()
message(STATUS "Semantic 68A1 first-live prefixes preserve all seven natural Gray-Blob activations.")
