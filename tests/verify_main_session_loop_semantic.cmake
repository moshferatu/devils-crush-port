# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported main-session-loop configuration: ${CONFIG}")
endif()

# 01:C154-01:C1B5, the main session loop. It waits for the frame, runs the nine
# calls a frame is made of, and asks the session state whether to run another.
#
#   C154  JSR $E321   the frame-sync wait      C16F  request $07:$03
#   C157  JSR $C4DE   the actor scan           C17A  request $08:$09
#   C15A  JSR $F261   the actor motion pass    C185  request $08:$15
#   C15D  request $08:$00                      C190  $2CC0.7, then <$53
#   C166  JSR $C3CB   the control-piece gate
#   C169  JSR $C49E
#   C16C  JSR $CA84   the ball/table gate
#
# The loop is a chain of crossings rather than one walk: a transcript that makes
# a call ends on it, so this is ten transcripts, one per call, each entered at
# the address the previous callee returns to. Eight of the nine calls land on a
# registered transcript; the ninth is the frame-sync wait, which is crossed for
# its own 21 cycles and then left to the interpreter, where the machine is meant
# to sit. 01:C154 is not an entry: control reaches it from the walk entered at
# 01:C190, which crosses it, and once at start-up by falling through from
# 01:C151, which stays emulated -- one execution and 21 master cycles in 3,600
# frames.
#
# Five arms leave the loop -- 01:C195 to the maximum-score ending, and the four
# session requests at 01:C1A2, 01:C1A9, 01:C1B0 and 01:C1B7 -- along with the
# <$53.4 arm at 01:C1BA. None executes on this route, all six are handed back,
# and 01:C195 in particular is live in real play: the score-saturation helper
# sets $2CC0.7 and the ending runs through it.
#
# `entries: 2649 of 2649` ten times over is also the shared-state measurement.
# No walk was declined at an interrupt horizon, which is what the counts would
# show: the longest of them is the tail at fourteen instructions and about 144
# master cycles, against an interrupt period near 5,068 on this route.
#
# **This is a comparison rather than a digest, and the reason is on the record.**
# Three times now a transcript has named the wrong cycle for a write while a
# pinned per-routine hash agreed with it, and each time what found it was
# requiring a replaced run's writes to match an emulated one's. So the route is
# recorded twice here, and the two traces must be identical: same addresses,
# same values, same cycles, same order. A digest could only say that this
# replacement still does what it did.
#
# The census is pinned as well, because a comparison cannot see route drift --
# both sides would drift together -- and the census is the artifact every other
# regression in this suite shares.
#
# `$202F` and `$2030` are the two bytes the four request setups write, and they
# are shared: every caller of the bank-call wrapper at 00:E241 stages its request
# there, so the trace interleaves this loop's eight writes a frame with every
# other caller's, and a wrong value, a wrong order or a shifted cycle anywhere on
# the route moves rows in it.
#
# The two `JSR` pushes each crossing makes are not checked here. They are checked
# by `dc_rom_registry_stack_page_writes`, which compares the whole route's writes
# to $2100-21FF against pure assembly and therefore covers all ten of them at
# once, which is the right place for it.
#
# The write filter matches the effective address, so zero page is named as
# $202F-$2030 rather than $2F-$30; written the short way the range selects
# nothing at all.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

set(emulated_census "${OUTPUT}.emulated.csv")
set(emulated_writes "${emulated_census}.writes.csv")
set(replaced_census "${OUTPUT}.replaced.csv")
set(replaced_writes "${replaced_census}.writes.csv")

function(record_route census writes)
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${census}"
            --work-ram-write-range "202F-2030"
            --work-ram-write-trace "${writes}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(
            FATAL_ERROR
            "Semantic main-session-loop recorder failed (${recorder_result}).\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    set(recorder_output "${recorder_output}" PARENT_SCOPE)
endfunction()

record_route("${emulated_census}" "${emulated_writes}")
set(emulated_output "${recorder_output}")

record_route(
    "${replaced_census}" "${replaced_writes}"
    --replace-hook "01:C157@all"
    --replace-hook "01:C15A@all"
    --replace-hook "01:C15D@all"
    --replace-hook "01:C166@all"
    --replace-hook "01:C169@all"
    --replace-hook "01:C16C@all"
    --replace-hook "01:C16F@all"
    --replace-hook "01:C17A@all"
    --replace-hook "01:C185@all"
    --replace-hook "01:C190@all"
)
set(replaced_output "${recorder_output}")

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:C157@all entries: 2649 of 2649"
    "Native replacement 01:C15A@all entries: 2649 of 2649"
    "Native replacement 01:C15D@all entries: 2649 of 2649"
    "Native replacement 01:C166@all entries: 2649 of 2649"
    "Native replacement 01:C169@all entries: 2649 of 2649"
    "Native replacement 01:C16C@all entries: 2649 of 2649"
    "Native replacement 01:C16F@all entries: 2649 of 2649"
    "Native replacement 01:C17A@all entries: 2649 of 2649"
    "Native replacement 01:C185@all entries: 2649 of 2649"
    "Native replacement 01:C190@all entries: 2649 of 2649"
    "Work-RAM write trace complete: 1169947 selected writes"
)
    string(FIND "${replaced_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic main-session-loop output omitted '${expected_line}'.\n"
            "${replaced_output}"
        )
    endif()
endforeach()

# A run that recorded nothing would compare equal to another that recorded
# nothing, so the emulated side is required to have traced the range too.
string(
    FIND "${emulated_output}"
    "Work-RAM write trace complete: 1169947 selected writes"
    emulated_trace_line
)
if(emulated_trace_line EQUAL -1)
    message(
        FATAL_ERROR
        "The emulated route did not trace the request bytes.\n"
        "${emulated_output}"
    )
endif()

foreach(artifact IN ITEMS census write)
    if(artifact STREQUAL "census")
        set(emulated_path "${emulated_census}")
        set(replaced_path "${replaced_census}")
    else()
        set(emulated_path "${emulated_writes}")
        set(replaced_path "${replaced_writes}")
    endif()
    file(SHA256 "${emulated_path}" emulated_sha256)
    file(SHA256 "${replaced_path}" replaced_sha256)
    if(NOT emulated_sha256 STREQUAL replaced_sha256)
        message(
            FATAL_ERROR
            "The replaced main session loop's ${artifact} differs from the "
            "emulated route.\nEmulated: ${emulated_sha256}\n"
            "Replaced: ${replaced_sha256}\n"
            "Re-record both and diff them; a single differing line names the "
            "instruction, the byte and the cycle the transcript got wrong."
        )
    endif()
endforeach()

file(SHA256 "${emulated_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(
        FATAL_ERROR
        "Semantic main-session-loop census artifact changed.\n"
        "Expected: ${expected_census_sha256}\nActual:   ${census_sha256}\n"
    )
endif()

file(REMOVE
    "${emulated_census}" "${emulated_writes}"
    "${replaced_census}" "${replaced_writes}")

message(
    STATUS
    "The main session loop runs natively on all 2,649 iterations across its ten "
    "entry points, with the request bytes it stages written in the same order, "
    "with the same values, at the same cycles as assembly."
)
