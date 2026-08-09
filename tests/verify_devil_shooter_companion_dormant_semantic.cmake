# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Devil-Shooter companion dormant regression is Release-only.")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "71a87a8e3d59e242da052cd7b7c4eacf413496c715c07fb150a615950115aa3e")
# Refreshed after the actor-loop and SAT slices moved where a transcript
# credits a write within its instruction. The census and call artifacts above
# pin the behaviour that matters and both still match: the call trace carries
# every call's body_master_cycles, full input/output register set and
# write_count, so the companion below has the same rows in the same order for
# the same calls. What moved is the per-write cycle attribution inside them.
set(expected_write_sha256 "fdbc03bd6b198ea758584a8e32ef5f9c52b4b85aea743c2eb56a5beac91f6865")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "02:53DD"
        --observe-hook "02:5397"
        --observe-hook "02:5408"
        --observe-hook "02:53C8"
        --observe-hook "02:53E4"
        --observe-hook "02:5441"
        --observe-hook "02:545D"
        --observe-hook "02:5456"
        --replace-hook "02:5397@all"
        --replace-hook "02:5408@all"
        --replace-hook "02:53C8@all"
        --replace-hook "02:53E4@all"
        --replace-hook "02:5441@all"
        --replace-hook "02:545D@all"
        --replace-hook "02:53DD@all"
        --replace-hook "02:5456@all"
        --routine-call-trace "${call_output}"
        --routine-replay "02:5408@10"
        --routine-replay "02:53C8@3"
        --routine-replay "02:5441@3"
        --routine-replay "02:53E4@3"
        --routine-replay "02:545D@10"
        --routine-replay "02:5397@10"
        --routine-replay "02:53DD@10"
        --routine-replay "02:5456@10"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Devil-Shooter companion recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:5397@all entries: 873 of 1260"
    "Native replacement 02:5408@all entries: 1015 of 1288"
    "Native replacement 02:53C8@all entries: 180 of 266"
    "Native replacement 02:53E4@all entries: 34 of 92"
    "Native replacement 02:5441@all entries: 174 of 267"
    "Native replacement 02:545D@all entries: 45 of 93"
    "Native replacement 02:53DD@all entries: 929 of 1055"
    "Native replacement 02:5456@all entries: 926 of 1085"
    "Observe hook 1 entries: 1055"
    "Observe hook 2 entries: 1260"
    "Observe hook 3 entries: 1288"
    "Observe hook 4 entries: 266"
    "Observe hook 5 entries: 92"
    "Observe hook 6 entries: 267"
    "Observe hook 7 entries: 93"
    "Observe hook 8 entries: 1085"
    "Routine replay 02:5408@10 verified: frame 949, 42 body cycles, 0 ordered writes"
    "Routine replay 02:53C8@3 verified: frame 945, 99 body cycles, 3 ordered writes"
    "Routine replay 02:5441@3 verified: frame 945, 99 body cycles, 3 ordered writes"
    "Routine replay 02:53E4@3 verified: frame 941, 33 body cycles, 1 ordered writes"
    "Routine replay 02:545D@10 verified: frame 965, 33 body cycles, 1 ordered writes"
    "Routine replay 02:5397@10 verified: frame 949, 75 body cycles, 2 ordered writes"
    "Routine replay 02:53DD@10 verified: frame 2145, 24 body cycles, 0 ordered writes"
    "Routine replay 02:5456@10 verified: frame 2141, 24 body cycles, 0 ordered writes"
    "Routine replay complete: 8 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Devil-Shooter companion output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Devil-Shooter companion ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 5397/5408/53C8/53E4/5441/545D/53DD/5456 Devil-Shooter companion leaves cover 4,176 event-horizon-safe calls.")
