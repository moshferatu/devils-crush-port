# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

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
            --observe-hook "03:66DC"
            --routine-call-trace "${calls}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "Three-Blobs RNG-loop branch recorder failed (${result}).\n${out}${err}")
    endif()
    set(recorder_output "${out}" PARENT_SCOPE)
endfunction()

record_route("${emulated_census}" "${emulated_calls}")
set(emulated_output "${recorder_output}")
record_route(
    "${replaced_census}" "${replaced_calls}"
    --replace-hook "03:66DC@all"
)
set(replaced_output "${recorder_output}")

foreach(output IN ITEMS emulated_output replaced_output)
    string(FIND "${${output}}" "Observe hook 1 entries: 60" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR
            "Three-Blobs RNG-loop observer did not close 60 entries.\n${${output}}")
    endif()
endforeach()

string(FIND "${replaced_output}"
    "Native replacement 03:66DC@all entries: 60 of 60" offset)
if(offset EQUAL -1)
    message(FATAL_ERROR
        "Three-Blobs RNG-loop branch was not native for every observed entry.\n${replaced_output}")
endif()

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
            "Three-Blobs RNG-loop branch ${artifact} differs from assembly.\n"
            "Emulated: ${emulated_sha256}\nReplaced: ${replaced_sha256}")
    endif()
endforeach()

message(STATUS "Three-Blobs RNG-loop branch matches assembly.")
