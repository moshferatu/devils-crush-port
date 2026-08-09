# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported SAT-submission configuration: ${CONFIG}")
endif()

# The driver that surrounds the already-native `08:40DF` emitter and `08:40C3`
# tail clear. It cannot be replaced as one routine, because a single
# transaction would have to subsume every nested `JSR $40DF` — twenty-one per
# frame at about 944 cycles each. Instead one shared walk starts at each of the
# five addresses control can arrive at with no replacement active: the
# routine's head at `08:4036` and the four returns from a `JSR $40DF`. Each
# walk runs until it reaches the next `JSR`, which the machine executes and
# which re-enters the native emitter.
#
# The scoped ranges are the driver's own work-RAM output: the VDC port shadow
# at $2006, the sprite count at $204E, and the two queue cursors at $2CBA and
# $2CBC that the drain loops decrement and re-read. Its VDC port writes have no
# work-RAM trace, so the census carries them — and it hashes the rendered
# framebuffer every frame, which is what a mispointed write address would show
# up in.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "e483f10651b97f84ef0786e5ac805b27181da141461b6fb697f81c94bb4abd9a"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "08:4036@all"
        --replace-hook
        "08:4063@all"
        --replace-hook
        "08:4075@all"
        --replace-hook
        "08:4088@all"
        --replace-hook
        "08:4097@all"
        --work-ram-write-range
        "2006-2006"
        --work-ram-write-range
        "204E-204E"
        --work-ram-write-range
        "2CBA-2CBC"
        --work-ram-write-trace
        "${write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic SAT-submission recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

# `08:4088` is entered 3,492 times on its own but only 2,567 times here: the
# other four walks reach it inline through the ball slot's two branches and
# absorb the difference.
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 08:4036@all entries: 3492 of 3492"
    "Native replacement 08:4063@all entries: 4427 of 4427"
    "Native replacement 08:4075@all entries: 16576 of 16576"
    "Native replacement 08:4088@all entries: 2567 of 2567"
    "Native replacement 08:4097@all entries: 50248 of 50248"
    "Work-RAM write trace complete: 233267 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic SAT-submission output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    else()
        set(artifact_path "${write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Semantic SAT-submission ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic 4036/4063/4075/4088/4097 SAT submission points the VDC, drains "
    "all three actor queues, and reaches the tail clear natively across all "
    "77,310 entries as instruction-boundary transcripts, with its port "
    "shadow, sprite count, and queue cursors and the whole state census — "
    "including every frame's rendered framebuffer — identical to assembly."
)
