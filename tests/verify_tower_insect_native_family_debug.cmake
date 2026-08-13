# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Tower-insect native family checkpoint requires Debug.")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
# This regression runs the Debug binary, and its census is now the same
# 9036a9de... the Release binary produces. It used to be a95d0c34..., the
# Debug-only baseline a hundred-odd verify scripts still carry in a branch
# nothing reaches -- this is the only test registered `CONFIGURATIONS Debug`,
# so it is the only one that ever compared against it.
#
# The two configurations agreeing is the right answer and the old split was
# the anomaly: a census is a record of machine state, and a build's
# optimization level has no business moving it. The value below is checked the
# strongest way a census baseline can be, by running the Debug binary with no
# hooks and no replacements at all -- an assembly-only route, which no
# transcript can reach and which produces exactly this hash.
#
# The call and write traces are identical to the Release ones.
set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "48ca8436bb293f964fb7a2f89ff441d31a6280615a6011f7b64e7685f0b41de2")
set(expected_write_sha256 "f169bf28c1f265139e016007c85b225cb883793850e4a609b01cb1cb2313898b")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "02:5F22"
        --replace-hook "02:5F22@all"
        --routine-replay "02:5F22@1"
        --observe-hook "03:60B3"
        --replace-hook "03:60B3@all"
        --routine-replay "03:60B3@1"
        --observe-hook "03:6054"
        --replace-hook "03:6054@all"
        --routine-replay "03:6054@1"
        --observe-hook "02:5F5F"
        --replace-hook "02:5F5F@all"
        --routine-replay "02:5F5F@1"
        --routine-call-trace "${call_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Tower-insect Debug family recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:5F22@all entries: 1 of 1"
    "Native replacement 03:60B3@all entries: 1 of 1"
    "Native replacement 03:6054@all entries: 6750 of 6750"
    "Native replacement 02:5F5F@all entries: 2504 of 2504"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 6750"
    "Observe hook 4 entries: 2504"
    "Routine replay 02:5F22@1 verified: frame 872, 192 body cycles, 7 ordered writes"
    "Routine replay 03:60B3@1 verified: frame 872, 249 body cycles, 10 ordered writes"
    "Routine replay 03:6054@1 verified: frame 958, 147 body cycles, 3 ordered writes"
    "Routine replay 02:5F5F@1 verified: frame 1086, 21 body cycles, 0 ordered writes"
    "Routine replay complete: 4 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Tower-insect Debug family output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Tower-insect Debug family ${artifact} changed.\nExpected: ${expected_sha256}\nActual: ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Tower-insect Debug family checkpoint verified all native source and child leaves.")
