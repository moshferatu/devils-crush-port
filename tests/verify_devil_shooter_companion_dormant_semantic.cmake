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
# The active parents now remain native across their timer calls, so those
# children no longer appear as separate execution-hook entries inside this
# combined run. Dedicated assembly/native sweeps over all 533 parent calls and
# all 539 timer calls keep their respective call and write artifacts identical;
# this digest pins the resulting combined-hook shape.
set(expected_call_sha256 "ac711b3b5d4781da6841ce2dc4b85583605a83e64e5f682ffece4b2f2183b6e0")
# Refreshed after the actor-loop and SAT slices moved where a transcript
# credits a write within its instruction. The census and call artifacts above
# pin the behaviour that matters and both still match: the call trace carries
# every call's body_master_cycles, full input/output register set and
# write_count, so the companion below has the same rows in the same order for
# the same calls. What moved is the per-write cycle attribution inside them.
#
# Refreshed again, and this time because two of those attributions were wrong.
# 02:5408's transcript credited `STA $3E00,X` at offset 48 and `INC $3980,X` at
# 75; a store lands on its instruction's final cycle, which puts them at 51 and
# 72. Every clean advance and reset call was three cycles out in both
# directions. Nothing here could have caught it -- this hash agreed with the
# mistake -- and it was found by tracing 02:5408 emulated and replaced and
# requiring the two to match. They now do, byte for byte, on all three
# artifacts, which is a stronger statement than the constant below.
#
# Refreshed a third time for the same reason at the other end of an
# instruction. 02:53C8 and 02:5441 are each a `JSR`, and their transcripts
# credited the two bytes it pushes at offset 6 -- the third and fourth cycle,
# which is where the silicon puts them. The core does not: the opcode runs
# `PUSH(PC >> 8); PUSH(PC);` and only afterwards `ADDCYC(6); LastCycle()`, so
# both bytes land on the instruction's first cycle. A store is credited at the
# end of its instruction and a push at the beginning, and a transcript has to
# say whichever one the emulator would have said. This hash agreed with the
# mistake too; what found it was requiring the whole route's writes to
# $2100-21FF to match pure assembly, where all 4,205,699 now do.
set(expected_write_sha256 "93ae519370adfbceb08bc4b84812d1f7d17ed5e2601f612ade3c21238f1768e6")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(instruction_profile_output "${OUTPUT}.profile.csv")

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
        --routine-replay "02:545D@1"
        --routine-replay "02:5397@10"
        --routine-replay "02:53DD@10"
        --routine-replay "02:5456@10"
        --instruction-profile "${instruction_profile_output}"
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
    "Native replacement 02:5397@all entries: 1260 of 1260"
    "Native replacement 02:5408@all entries: 1288 of 1288"
    "Native replacement 02:53C8@all entries: 265 of 266"
    "Native replacement 02:53E4@all entries: 4 of 7"
    "Native replacement 02:5441@all entries: 266 of 267"
    "Native replacement 02:545D@all entries: 1 of 1"
    "Native replacement 02:53DD@all entries: 1055 of 1055"
    "Native replacement 02:5456@all entries: 1085 of 1085"
    "Observe hook 1 entries: 1055"
    "Observe hook 2 entries: 1260"
    "Observe hook 3 entries: 1288"
    "Observe hook 4 entries: 266"
    "Observe hook 5 entries: 7"
    "Observe hook 6 entries: 267"
    "Observe hook 7 entries: 1"
    "Observe hook 8 entries: 1085"
    "Routine replay 02:5408@10 verified: frame 949, 42 body cycles, 0 ordered writes"
    "Routine replay 02:53C8@3 verified: frame 945, 99 body cycles, 3 ordered writes"
    "Routine replay 02:5441@3 verified: frame 945, 99 body cycles, 3 ordered writes"
    "Routine replay 02:53E4@3 verified: frame 941, 33 body cycles, 1 ordered writes"
    "Routine replay 02:545D@1 verified: frame 2131, 33 body cycles, 1 ordered writes"
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

file(READ "${instruction_profile_output}" instruction_profile)
string(FIND "${instruction_profile}" "02,53FB,3,63," remaining_left_return_offset)
if(remaining_left_return_offset EQUAL -1)
    message(FATAL_ERROR "Devil-Shooter left companion return profile changed.")
endif()
string(FIND "${instruction_profile}" "02,5474," remaining_right_return_offset)
if(NOT remaining_right_return_offset EQUAL -1)
    message(FATAL_ERROR "Devil-Shooter right companion return remains emulated.")
endif()

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

message(STATUS "Semantic 5397/5408/53C8/53E4/5441/545D/53DD/5456 Devil-Shooter companion routes admit 5,224 recorded hook entries.")
