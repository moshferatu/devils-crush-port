# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(CONFIG STREQUAL "Debug")
    # This Front-Spider observer route lands on the same normalized
    # framebuffer-placement variant as the owning family regression.
    set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
elseif(CONFIG STREQUAL "Release")
    set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
else()
    message(FATAL_ERROR "Unsupported Front-Spider scheduler-seam configuration: ${CONFIG}")
endif()
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
            --observe-hook "03:64D2"
            --observe-hook "03:64DD"
            --observe-hook "03:64ED"
            --routine-call-trace "${calls}"
            --routine-replay "03:64D2@1"
            --routine-replay "03:64DD@1"
            --routine-replay "03:64ED@1"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Front-Spider scheduler-seam recorder failed (${result}).\n${out}${err}")
    endif()
    set(recorder_output "${out}" PARENT_SCOPE)
endfunction()

record_route("${emulated_census}" "${emulated_calls}")
set(emulated_output "${recorder_output}")
record_route(
    "${replaced_census}"
    "${replaced_calls}"
    --replace-hook "03:64D2@all"
    --replace-hook "03:64DD@all"
    --replace-hook "03:64ED@all"
)
set(replaced_output "${recorder_output}")

foreach(output IN ITEMS emulated_output replaced_output)
    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Observe hook 1 entries: 36"
        "Observe hook 2 entries: 36"
        "Observe hook 3 entries: 144"
        "Routine call trace complete: 216 calls, 1172 writes"
        "Routine replay 03:64D2@1 verified: frame 2182, 135 body cycles, 5 ordered writes"
        "Routine replay 03:64DD@1 verified: frame 2194, 135 body cycles, 5 ordered writes"
        "Routine replay 03:64ED@1 verified: frame 2206, 135 body cycles, 5 ordered writes"
        "Routine replay complete: 3 state-normalized calls"
    )
        string(FIND "${${output}}" "${expected_line}" offset)
        if(offset EQUAL -1)
            message(FATAL_ERROR "Front-Spider scheduler-seam output omitted '${expected_line}'.\n${${output}}")
        endif()
    endforeach()
endforeach()

foreach(expected_line IN ITEMS
    "Native replacement 03:64D2@all entries: 36 of 36"
    "Native replacement 03:64DD@all entries: 36 of 36"
    "Native replacement 03:64ED@all entries: 144 of 144"
)
    string(FIND "${replaced_output}" "${expected_line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Front-Spider scheduler-seam output omitted '${expected_line}'.\n${replaced_output}")
    endif()
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
            "Front-Spider scheduler-seam ${artifact} differs from assembly.\n"
            "Emulated: ${emulated_sha256}\nReplaced: ${replaced_sha256}"
        )
    endif()
endforeach()

file(SHA256 "${emulated_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(FATAL_ERROR "Front-Spider scheduler-seam census changed: ${census_sha256}")
endif()
message(STATUS "Front-Spider scheduler call seams preserve the exact 216-call/1,172-write artifact.")
