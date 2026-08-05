# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported drain-threshold configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9e928058264849ba617b7c36697cb24779b28a10072c6c92c6b1786d72f284a2")
set(expected_call_sha256 "0416efdc96c110a6a029e34d9e6c00a6f11dc338534d793cb57ed476a4ec3664")
set(expected_write_sha256 "d96cbef47b9f30cb83512a12eb135951ac432f6f1572308556dc2b238ccc2341")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "3000:3380:03"
        --work-ram-patch "3000:3300:00"
        --observe-hook "01:DD6E"
        --replace-hook "01:DD6E@all"
        --routine-call-trace "${call_output}"
        --routine-replay "01:DD6E@1744"
        --rom "${ROM}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Semantic drain-threshold recorder failed (${result}).\n${out}${err}")
endif()

foreach(line IN ITEMS
    "Native replacement 01:DD6E@all entries: 1744 of 1744"
    "Routine call trace complete: 1744 calls, 845 writes"
    "Routine replay 01:DD6E@1744 verified: frame 3000, 78 body cycles, 0 ordered writes"
)
    string(FIND "${out}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Semantic drain-threshold output omitted '${line}'.\n${out}${err}")
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
        message(FATAL_ERROR "Semantic drain-threshold ${kind} artifact changed.\nExpected: ${expected_${kind}_sha256}\nActual:   ${actual}\n")
    endif()
endforeach()

message(STATUS "Semantic DD6E drain threshold preserves all ordinary returns and the injected carry-set drain branch.")
