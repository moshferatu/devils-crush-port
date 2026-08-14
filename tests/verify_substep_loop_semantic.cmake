# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported substep-loop configuration: ${CONFIG}")
endif()

# 01:C721-01:C770, the ball's substep loop, across eleven entry points. It is
# the consumer of the substep split at 01:DC95: `$57` is the number of substeps
# the split asked for this frame, `$56` counts them off, `$59` caps the loop at
# sixteen times round, and the far end runs the path-length accumulator at
# 01:C8F3, the damping at 01:DD09 and the drain test at 01:DD6E before
# tail-jumping back into 01:DC95 to arm the next frame.
#
# Every routine it calls was already in the registry, so what this walk converts
# is the control flow between them. It is the first family in the port whose
# value is mostly in its calls rather than in its arithmetic: eight of its
# thirty-one instructions are `JSR`s, all eight are crossed, and all eight land
# on a registered entry, so the machine leaves each transcript already standing
# on the next one.
#
# Ten of the eleven entry points are the address one of those calls returns to.
# The eleventh is the head at 01:C721, which is not reached by a call at all --
# 01:C711 writes it into `$3F00,X`/`$3F80,X`, where the coroutine scheduler at
# 01:C5C9 keeps this actor's resume address.
#
# Three things are handed back on purpose:
#
#   01:C756  the `BBR3 <$77`. `BBR` is `LD_ZP` wrapping `JR`, so the core
#            crosses it in four clock steps where a checkpoint crosses it in
#            two, and those steps are where it tests `next_event`. Both of its
#            arms, 01:C759 and 01:C75E, are entry points because the core
#            resolves the instruction itself. It is the only opcode in the range
#            with a low nibble of `F`, and leaving it interpreted costs about
#            64,000 master cycles.
#   01:C728, 01:C753, 01:C773
#            branch arms this route never takes -- the off-table exit through
#            01:C8D7 twice over, and the drain test reporting a drained ball.
#            None of the three executed once in 3,600 frames.
#
# Shared state was measured on a pure-assembly write trace of every write to
# `$56`, `$57`, `$59` and `$3C80-$3CFF`. `$57` is written only by 01:DC95's five
# stores; `$56` and `$59` only by this loop, the near-identical pre-loop at
# 01:C6C9 and the startup `TII`; and `$3C80,X` by bank 01, 02, 03 and 08 game
# code plus 00:F1D7 and 00:F23D, which are inside two of the callees. Nothing in
# the VDC handler, the timer driver or the music sequencer writes any of it, so
# no interrupt taken inside one of these windows can change what the walk read.
#
# The ranges below are the loop's own two counters, the whole stack page, and
# the ball's four signed sixteen-bit motion components. The stack page is the
# range that matters most here: a crossed `JSR` pushes its two return bytes at
# the instruction's start offset, so every one of the eight crossings appears in
# the trace with its address, its value and its master cycle, and a cycle model
# that were wrong by one would move them. `$3400-$35FF` is what the loop exists
# to integrate, and it is written by the callees rather than by the walk, so it
# is the arm that would catch a transcript that produced its own artifacts
# exactly and still shifted the machine's event phase around them.
#
# The write filter matches the effective address, so zero page is named as
# $2056 and $2059 rather than $56 and $59; written the short way the two ranges
# select nothing at all. The range order is part of the artifact, because the
# trace is emitted in the order the ranges are given.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "43b11e2704651031c6dbeb9ddde5c30957f64d90f63d9fa0a7debb304be574e4"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "01:C721@all"
        --replace-hook
        "01:C72E@all"
        --replace-hook
        "01:C742@all"
        --replace-hook
        "01:C74C@all"
        --replace-hook
        "01:C759@all"
        --replace-hook
        "01:C75C@all"
        --replace-hook
        "01:C75E@all"
        --replace-hook
        "01:C761@all"
        --replace-hook
        "01:C768@all"
        --replace-hook
        "01:C76B@all"
        --replace-hook
        "01:C76E@all"
        --work-ram-write-range
        "2056-2056"
        --work-ram-write-range
        "2059-2059"
        --work-ram-write-range
        "2100-21FF"
        --work-ram-write-range
        "3400-35FF"
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
        "Semantic substep-loop recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:C721@all entries: 2342 of 2342"
    "Native replacement 01:C72E@all entries: 2342 of 2342"
    "Native replacement 01:C742@all entries: 3267 of 3267"
    "Native replacement 01:C74C@all entries: 3039 of 3039"
    "Native replacement 01:C759@all entries: 274 of 274"
    "Native replacement 01:C75C@all entries: 274 of 274"
    "Native replacement 01:C75E@all entries: 2765 of 2765"
    "Native replacement 01:C761@all entries: 2993 of 2993"
    "Native replacement 01:C768@all entries: 2340 of 2340"
    "Native replacement 01:C76B@all entries: 2340 of 2340"
    "Native replacement 01:C76E@all entries: 2340 of 2340"
    "Work-RAM write trace complete: 4239583 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic substep-loop output omitted '${expected_line}'.\n"
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
            "Semantic substep-loop ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "The ball's substep loop runs all eleven entry points natively across "
    "24,316 admitted entries, with its counters, every return address its "
    "eight crossings push and the motion it integrates identical to assembly."
)
