# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Giant-Face helmet outer initializer configuration: ${CONFIG}")
endif()

function(verify_outer_initializer label hook expected_call expected_write)
    set(census_output "${OUTPUT}.${label}.csv")
    set(call_output "${census_output}.calls.csv")
    set(write_output "${call_output}.writes.csv")
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${census_output}"
            --observe-hook "${hook}"
            --replace-hook "${hook}@all"
            --routine-call-trace "${call_output}"
            --routine-replay "${hook}@1"
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(FATAL_ERROR "Semantic Giant-Face ${label} outer initializer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
    endif()
    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Native replacement ${hook}@all entries: 1 of 1"
        "Observe hook 1 entries: 1"
        "Routine call trace complete: 1 calls, 9 writes"
        "Routine replay ${hook}@1 verified: frame 872, 273 body cycles, 9 ordered writes"
        "Routine replay complete: 1 state-normalized calls"
    )
        string(FIND "${recorder_output}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Semantic Giant-Face ${label} outer initializer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
        endif()
    endforeach()
    file(SHA256 "${census_output}" actual_census)
    if(NOT actual_census STREQUAL "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
        message(FATAL_ERROR "Semantic Giant-Face ${label} outer initializer census artifact changed.\nActual: ${actual_census}")
    endif()
    file(SHA256 "${call_output}" actual_call)
    if(NOT actual_call STREQUAL expected_call)
        message(FATAL_ERROR "Semantic Giant-Face ${label} outer initializer call artifact changed.\nExpected: ${expected_call}\nActual:   ${actual_call}")
    endif()
    file(SHA256 "${write_output}" actual_write)
    if(NOT actual_write STREQUAL expected_write)
        message(FATAL_ERROR "Semantic Giant-Face ${label} outer initializer write artifact changed.\nExpected: ${expected_write}\nActual:   ${actual_write}")
    endif()
endfunction()

verify_outer_initializer(
    "left"
    "02:4A5C"
    "481f82d66cb350a13d24e65b14bedc660b3dda00ad7bbfb928e63a2e8e71bbfd"
    "db5e7873f296564b9986a6b14c412d26276f185f24a7042c58ad3d151a175840"
)
verify_outer_initializer(
    "right"
    "02:4A64"
    "20dcac3547950eb965bc373c02d1602029449bc333dba9a47ab39e985fed7e49"
    "dab0207aeba21b56fb2e49007cc13af4d9a6ee34f1bcf092871b76fa83e6d599"
)
message(STATUS "Semantic outer Giant-Face helmet initializers cover both fixed wrappers.")
