# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported Three-Blobs RNG-initialization configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(emulated_census "${OUTPUT}.emulated.csv")
set(emulated_calls "${emulated_census}.calls.csv")
set(emulated_writes "${emulated_calls}.writes.csv")
set(replaced_census "${OUTPUT}.replaced.csv")
set(replaced_calls "${replaced_census}.calls.csv")
set(replaced_writes "${replaced_calls}.writes.csv")

function(record_route census calls)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${census}"
            --observe-hook "03:6678"
            --observe-hook "03:667B"
            --observe-hook "03:6697"
            --observe-hook "03:669A"
            --routine-call-trace "${calls}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "Three-Blobs RNG-initialization recorder failed (${result}).\n${out}${err}")
    endif()
    set(recorder_output "${out}" PARENT_SCOPE)
endfunction()

record_route("${emulated_census}" "${emulated_calls}")
set(emulated_output "${recorder_output}")
record_route(
    "${replaced_census}" "${replaced_calls}"
    --replace-hook "03:6678@all"
    --replace-hook "03:667B@all"
    --replace-hook "03:6697@all"
    --replace-hook "03:669A@all"
)
set(replaced_output "${recorder_output}")

foreach(line IN ITEMS
    "Native replacement 03:6678@all entries: 9 of 9"
    "Native replacement 03:667B@all entries: 9 of 9"
    "Native replacement 03:6697@all entries: 69 of 69"
    "Native replacement 03:669A@all entries: 69 of 69"
    "Routine call trace complete: 156 calls, 2509 writes"
)
    string(FIND "${replaced_output}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR
            "Three-Blobs RNG-initialization output omitted '${line}'.\n${replaced_output}")
    endif()
endforeach()

foreach(output IN ITEMS emulated_output replaced_output)
    foreach(hook_and_count IN ITEMS "1;9" "2;9" "3;69" "4;69")
        list(GET hook_and_count 0 hook)
        list(GET hook_and_count 1 count)
        string(FIND "${${output}}" "Observe hook ${hook} entries: ${count}" offset)
        if(offset EQUAL -1)
            message(FATAL_ERROR
                "Three-Blobs RNG observer ${hook} did not close ${count} calls.\n${${output}}")
        endif()
    endforeach()
endforeach()

foreach(artifact IN ITEMS census call write)
    if(artifact STREQUAL "census")
        set(emulated_path "${emulated_census}")
        set(replaced_path "${replaced_census}")
    elseif(artifact STREQUAL "call")
        set(emulated_path "${emulated_calls}")
        set(replaced_path "${replaced_calls}")
    else()
        set(emulated_path "${emulated_writes}")
        set(replaced_path "${replaced_writes}")
    endif()
    file(SHA256 "${emulated_path}" emulated_sha256)
    file(SHA256 "${replaced_path}" replaced_sha256)
    if(NOT emulated_sha256 STREQUAL replaced_sha256)
        message(FATAL_ERROR
            "Three-Blobs RNG-initialization ${artifact} differs from assembly.\n"
            "Emulated: ${emulated_sha256}\nReplaced: ${replaced_sha256}")
    endif()
endforeach()

file(SHA256 "${emulated_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(FATAL_ERROR "Three-Blobs RNG-initialization census changed: ${census_sha256}")
endif()

message(STATUS
    "Three-Blobs RNG consumers and their movement/scheduler continuations match assembly.")
