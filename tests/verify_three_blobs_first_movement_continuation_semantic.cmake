# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported Three-Blobs first-movement continuation configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "526149c4a9e2a54c71f99ec496e2ff2e649ff5b172f535e63ca3f475458f5c60")
set(expected_write_sha256 "06a0fc285b44827cb835596c78be3ebe6af325d2d4ccdc6f63430759e24e08c5")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:66B5"
        --replace-hook "03:66B5@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:66B5@1"
        --rom "${ROM}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "Semantic Three-Blobs first-movement continuation recorder failed (${result}).\n${out}${err}")
endif()

foreach(line IN ITEMS
    "Native replacement 03:66B5@all entries: 1107 of 1107"
    "Routine call trace complete: 1107 calls, 9595 writes"
    "Routine replay 03:66B5@1 verified: frame 1229, 249 body cycles, 5 ordered writes"
)
    string(FIND "${out}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR
            "Three-Blobs first-movement continuation output omitted '${line}'.\n${out}${err}")
    endif()
endforeach()

foreach(kind IN ITEMS census call write)
    if(kind STREQUAL "census")
        set(path "${OUTPUT}")
        set(expected "${expected_census_sha256}")
    elseif(kind STREQUAL "call")
        set(path "${call_output}")
        set(expected "${expected_call_sha256}")
    else()
        set(path "${write_output}")
        set(expected "${expected_write_sha256}")
    endif()
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR
            "Three-Blobs first-movement continuation ${kind} changed.\nExpected: ${expected}\nActual:   ${actual}")
    endif()
endforeach()

message(STATUS
    "Semantic 66B5 preserves all direct, x-expiry, and y-expiry movement continuations.")
