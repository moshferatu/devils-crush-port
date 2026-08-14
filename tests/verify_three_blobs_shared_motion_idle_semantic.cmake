# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Three-Blobs shared-motion configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "5a8c5d9163801290a703a8dc1f98b480b3e6a820a8d52b66286314de897dae98")
set(expected_write_sha256 "c1f66cbd0ce16c602ec4ba9e1fef455e8e290447b9342807fb18683aba6b59cf")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:676B"
        --replace-hook "03:676B@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:676B@1"
        --rom "${ROM}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Semantic Three-Blobs shared-motion recorder failed (${result}).\n${out}${err}")
endif()
foreach(line IN ITEMS
    "Native replacement 03:676B@all entries: 1950 of 1965"
    "Routine call trace complete: 1965 calls, 4380 writes"
    "Routine replay 03:676B@1 verified: frame 1229, 147 body cycles, 1 ordered writes"
)
    string(FIND "${out}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Semantic Three-Blobs shared-motion output omitted '${line}'.\n${out}${err}")
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
        message(FATAL_ERROR "Semantic Three-Blobs shared-motion ${kind} artifact changed.")
    endif()
endforeach()
message(STATUS "Semantic 676B shared motion preserves its scene-clear appearance/no-hit leaf.")
