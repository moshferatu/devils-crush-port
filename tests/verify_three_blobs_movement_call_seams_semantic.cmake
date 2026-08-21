# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Three-Blobs movement-seam configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(emulated_census "${OUTPUT}.emulated.csv")
set(emulated_writes "${emulated_census}.writes.csv")
set(emulated_calls "${emulated_census}.calls.csv")
set(emulated_call_writes "${emulated_calls}.writes.csv")
set(replaced_census "${OUTPUT}.replaced.csv")
set(replaced_writes "${replaced_census}.writes.csv")
set(replaced_calls "${replaced_census}.calls.csv")
set(replaced_call_writes "${replaced_calls}.writes.csv")

function(record_route census writes calls)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${census}"
            --observe-hook "03:66BF"
            --observe-hook "03:66E2"
            --observe-hook "03:670F"
            --observe-hook "03:6729"
            --observe-hook "03:6740"
            --routine-call-trace "${calls}"
            --work-ram-write-range 2100-21FF
            --work-ram-write-trace "${writes}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Three-Blobs movement-seam recorder failed (${result}).\n${out}${err}")
    endif()
    set(recorder_output "${out}" PARENT_SCOPE)
endfunction()

record_route("${emulated_census}" "${emulated_writes}" "${emulated_calls}")
set(emulated_output "${recorder_output}")
record_route(
    "${replaced_census}" "${replaced_writes}" "${replaced_calls}"
    --replace-hook "03:66BF@all"
    --replace-hook "03:66D3@all"
    --replace-hook "03:66D6@all"
    --replace-hook "03:66D9@all"
    --replace-hook "03:66E2@all"
    --replace-hook "03:670A@all"
    --replace-hook "03:670C@all"
    --replace-hook "03:670F@all"
    --replace-hook "03:6726@all"
    --replace-hook "03:6729@all"
    --replace-hook "03:673D@all"
    --replace-hook "03:6740@all"
)
set(replaced_output "${recorder_output}")

foreach(expected_line IN ITEMS
    "Native replacement 03:66BF@all entries: 60 of 60"
    "Native replacement 03:66D3@all entries: 720 of 720"
    "Native replacement 03:66D6@all entries: 720 of 720"
    "Native replacement 03:66D9@all entries: 720 of 720"
    "Native replacement 03:66E2@all entries: 6 of 6"
    "Native replacement 03:670A@all entries: 204 of 204"
    "Native replacement 03:670C@all entries: 204 of 204"
    "Native replacement 03:670F@all entries: 204 of 204"
    "Native replacement 03:6726@all entries: 204 of 204"
    "Native replacement 03:6729@all entries: 204 of 204"
    "Native replacement 03:673D@all entries: 204 of 204"
    "Native replacement 03:6740@all entries: 204 of 204"
)
    string(FIND "${replaced_output}" "${expected_line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Three-Blobs movement-seam output omitted '${expected_line}'.\n${replaced_output}")
    endif()
endforeach()

foreach(hook_and_count IN ITEMS "3;204" "4;204" "5;204")
    list(GET hook_and_count 0 hook)
    list(GET hook_and_count 1 count)
    foreach(output IN ITEMS emulated_output replaced_output)
        string(FIND "${${output}}" "Observe hook ${hook} entries: ${count}" offset)
        if(offset EQUAL -1)
            message(FATAL_ERROR
                "Three-Blobs arithmetic observer ${hook} did not close ${count} calls.\n${${output}}"
            )
        endif()
    endforeach()
endforeach()

foreach(output IN ITEMS emulated_output replaced_output)
    string(FIND "${${output}}" "Observe hook 2 entries: 6" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Three-Blobs alternate reset observer did not close six calls.\n${${output}}")
    endif()
endforeach()

foreach(output IN ITEMS emulated_output replaced_output)
    string(FIND "${${output}}" "Observe hook 1 entries: 60" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Three-Blobs y-expiry observer did not close 60 calls.\n${${output}}")
    endif()
endforeach()

string(REGEX MATCH "Work-RAM write trace complete: ([0-9]+) selected writes" count_line "${emulated_output}")
if(count_line STREQUAL "" OR CMAKE_MATCH_1 LESS 1000000)
    message(FATAL_ERROR "The emulated movement-seam route did not trace the stack page.\n${emulated_output}")
endif()
set(stack_write_count "${CMAKE_MATCH_1}")

foreach(artifact IN ITEMS census write call call_write)
    if(artifact STREQUAL "census")
        set(emulated_path "${emulated_census}")
        set(replaced_path "${replaced_census}")
    elseif(artifact STREQUAL "write")
        set(emulated_path "${emulated_writes}")
        set(replaced_path "${replaced_writes}")
    elseif(artifact STREQUAL "call")
        set(emulated_path "${emulated_calls}")
        set(replaced_path "${replaced_calls}")
    else()
        set(emulated_path "${emulated_call_writes}")
        set(replaced_path "${replaced_call_writes}")
    endif()
    file(SHA256 "${emulated_path}" emulated_sha256)
    file(SHA256 "${replaced_path}" replaced_sha256)
    if(NOT emulated_sha256 STREQUAL replaced_sha256)
        message(FATAL_ERROR
            "Three-Blobs movement-seam ${artifact} differs from assembly.\n"
            "Emulated: ${emulated_sha256}\nReplaced: ${replaced_sha256}"
        )
    endif()
endforeach()

file(SHA256 "${emulated_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(FATAL_ERROR "Three-Blobs movement-seam census changed: ${census_sha256}")
endif()
message(STATUS "Three-Blobs movement call seams preserve ${stack_write_count} stack-page writes.")
