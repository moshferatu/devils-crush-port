# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported flipper post-response configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(emulated_census "${OUTPUT}.emulated.csv")
set(emulated_calls "${emulated_census}.calls.csv")
set(replaced_census "${OUTPUT}.replaced.csv")
set(replaced_calls "${replaced_census}.calls.csv")
set(abandonments "${OUTPUT}.abandonments.csv")

function(record_route census calls)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${census}"
            --observe-hook "01:D5B8"
            --observe-hook "01:D5BB"
            --observe-hook "01:D5BE"
            --observe-hook "01:D5C1"
            --observe-hook "01:D5C4"
            --observe-hook "01:D5D8"
            --routine-call-trace "${calls}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Flipper post-response recorder failed (${result}).\n${out}${err}")
    endif()
    set(recorder_output "${out}" PARENT_SCOPE)
endfunction()

record_route("${emulated_census}" "${emulated_calls}")
set(emulated_output "${recorder_output}")
record_route(
    "${replaced_census}"
    "${replaced_calls}"
    --replace-hook "01:D5B8@all"
    --replace-hook "01:D5BB@all"
    --replace-hook "01:D5BE@all"
    --replace-hook "01:D5C1@all"
    --replace-hook "01:D5C4@all"
    --replace-hook "01:D5D8@all"
    --abandonment-census "${abandonments}"
)
set(replaced_output "${recorder_output}")

foreach(output IN ITEMS emulated_output replaced_output)
    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Observe hook 1 entries: 43"
        "Observe hook 2 entries: 43"
        "Observe hook 3 entries: 43"
        "Observe hook 4 entries: 43"
        "Observe hook 5 entries: 43"
        "Observe hook 6 entries: 20"
        "Routine call trace complete: 235 calls, 160392 writes"
    )
        string(FIND "${${output}}" "${expected_line}" offset)
        if(offset EQUAL -1)
            message(FATAL_ERROR "Flipper post-response output omitted '${expected_line}'.\n${${output}}")
        endif()
    endforeach()
endforeach()

foreach(expected_line IN ITEMS
    "Native replacement 01:D5B8@all entries: 43 of 43"
    "Native replacement 01:D5BB@all entries: 43 of 43"
    "Native replacement 01:D5BE@all entries: 43 of 43"
    "Native replacement 01:D5C1@all entries: 43 of 43"
    "Native replacement 01:D5C4@all entries: 43 of 43"
    "Native replacement 01:D5D8@all entries: 20 of 20"
    "Abandonment census complete: no transcript was abandoned"
)
    string(FIND "${replaced_output}" "${expected_line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Flipper post-response output omitted '${expected_line}'.\n${replaced_output}")
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
        set(emulated_path "${emulated_calls}.writes.csv")
        set(replaced_path "${replaced_calls}.writes.csv")
    endif()
    file(SHA256 "${emulated_path}" emulated_sha256)
    file(SHA256 "${replaced_path}" replaced_sha256)
    if(NOT emulated_sha256 STREQUAL replaced_sha256)
        message(FATAL_ERROR
            "Flipper post-response ${artifact} differs from assembly.\n"
            "Emulated: ${emulated_sha256}\nReplaced: ${replaced_sha256}"
        )
    endif()
endforeach()

file(SHA256 "${emulated_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(FATAL_ERROR "Flipper post-response census changed: ${census_sha256}")
endif()
message(STATUS "Semantic 01:D5B8/D5BB/D5BE/D5C1/D5C4/D5D8 preserves the exact 235-call/160,392-write artifact.")
