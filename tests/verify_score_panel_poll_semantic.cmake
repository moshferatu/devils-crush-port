# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported score-panel configuration: ${CONFIG}")
endif()

# The bank-09 score-panel family includes both ornament animations. 09:70C1
# hands the left actor to the coroutine scheduler, 09:70C4 and 09:71EA are the
# left/right eight-tick frame steppers, and 09:7237-09:7268 is the redraw poll. Four
# work RAM gates -- the
# in-progress flag $2DBB, the session handshake $2D53, the hold-off countdown
# $2D70 and the panel dirty bit $2CC0.0 -- in front of the redraw at 09:7266
# that ROUTINE_MAP.md records converting the score and driving the VDC through
# 09:72FD. On this route the poll declines the redraw 2,616 times out of 2,649,
# while the 09:72FD glyph writer handles all 376 two-by-two digits requested by
# the admitted redraws.  Once the redraw head/loops are native, 306 of those
# glyphs are composed inside their bounded parents, leaving 70 independently
# entered glyph calls for the external observer.
#
# **A routine whose product is a decision needs an oracle for the decision, not
# just for the bytes.** The poll writes one byte, once per redraw, thirty-three
# times in 3,600 frames; a write-range comparison alone would be nearly empty
# and would pass just as happily if the transcript returned the wrong flags on
# every one of the 2,616 calls that write nothing at all. Four artifacts, each
# for a different claim:
#
#   the call trace     counts the eight observed entries. The three new loop
#                      transactions deliberately close at their natural
#                      re-entry seam, so their call scopes are not comparable
#                      to assembly's enclosing 72EA scope. The census and the
#                      two ordered-write artifacts below remain the parity
#                      oracle for the composed child work.
#   $202D-$2036        the zero-page scratch the redraw builds its VRAM cursor
#                      and digit counters in. Empty unless the poll admits the
#                      redraw, so this range is what says the gate opened on the
#                      same thirty-three calls.
#   $2CC0, $2D52-$2DBB the flag the poll clears, and the gates the body writes
#                      back: the ball count, the palette byte $2D6F, the
#                      countdown and the in-progress flag.
#   $0000-$0003        the VDC base ports. The redraw's whole visible output is
#                      a tile stream through these four registers, and a poll
#                      that consumed the dirty bit one call early or late would
#                      move it. Device writes are invisible to a work-RAM
#                      census, so they need a bus trace to be localised rather
#                      than merely caught.
#
# The census is pinned on top, because a comparison cannot see route drift --
# both sides would drift together.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

set(emulated_census "${OUTPUT}.emulated.csv")
set(emulated_writes "${emulated_census}.writes.csv")
set(emulated_bus "${emulated_census}.bus.csv")
set(replaced_census "${OUTPUT}.replaced.csv")
set(replaced_writes "${replaced_census}.writes.csv")
set(replaced_bus "${replaced_census}.bus.csv")
set(emulated_calls "${OUTPUT}.emulated.calls.csv")
set(replaced_calls "${OUTPUT}.replaced.calls.csv")

function(record_route census writes bus calls)
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${census}"
            --work-ram-write-range 202D-2036
            --work-ram-write-range 2CC0-2CC0
            --work-ram-write-range 2D52-2DBB
            --work-ram-write-range 3980-3AFF
            --work-ram-write-trace "${writes}"
            --bus-write-range 0000-0003
            --bus-write-trace "${bus}"
            --observe-hook "09:70C1"
            --observe-hook "09:70C4"
            --observe-hook "09:71EA"
            --observe-hook "09:7237"
            --observe-hook "09:72FD"
            --observe-hook "09:727D"
            --observe-hook "09:72A2"
            --observe-hook "09:72BD"
            --observe-hook "09:72C5"
            --routine-call-trace "${calls}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(
            FATAL_ERROR
            "Semantic score-panel recorder failed (${recorder_result}).\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    set(recorder_output "${recorder_output}" PARENT_SCOPE)
endfunction()

record_route(
    "${emulated_census}" "${emulated_writes}" "${emulated_bus}"
    "${emulated_calls}")
set(emulated_output "${recorder_output}")

record_route(
    "${replaced_census}" "${replaced_writes}" "${replaced_bus}"
    "${replaced_calls}"
    --replace-hook "09:70C1@all"
    --replace-hook "09:70C4@all"
    --replace-hook "09:71EA@all"
    --replace-hook "09:7237@all"
    --replace-hook "09:72FD@all"
    --replace-hook "09:727D@all"
    --replace-hook "09:72A2@all"
    --replace-hook "09:72BD@all"
    --replace-hook "09:72C5@all"
)
set(replaced_output "${recorder_output}")

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 09:70C1@all entries: 1 of 1"
    "Native replacement 09:70C4@all entries: 669 of 669"
    "Native replacement 09:71EA@all entries: 670 of 670"
    "Native replacement 09:7237@all entries: 2649 of 2649"
    "Native replacement 09:72FD@all entries: 2 of 2"
    "Native replacement 09:727D@all entries: 34 of 34"
    "Native replacement 09:72A2@all entries: 105 of 105"
    "Native replacement 09:72BD@all entries: 133 of 133"
    "Native replacement 09:72C5@all entries: 34 of 34"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 669"
    "Observe hook 3 entries: 670"
    "Observe hook 4 entries: 2649"
    "Observe hook 5 entries: 2"
    "Observe hook 6 entries: 34"
    "Observe hook 7 entries: 105"
    "Observe hook 8 entries: 133"
    "Observe hook 9 entries: 34"
    "Routine call trace complete: 4297 calls, 111202 writes"
)
    string(FIND "${replaced_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic score-panel output omitted '${expected_line}'.\n"
            "${replaced_output}"
        )
    endif()
endforeach()

# A run that recorded nothing would compare equal to another that recorded
# nothing, so the emulated side is required to have traced both channels.
string(
    REGEX MATCH
    "Work-RAM write trace complete: ([0-9]+) selected writes"
    emulated_count_line
    "${emulated_output}"
)
if(emulated_count_line STREQUAL "" OR CMAKE_MATCH_1 LESS 2000000)
    message(
        FATAL_ERROR
        "The emulated route did not trace the panel's work RAM.\n"
        "${emulated_output}"
    )
endif()
set(panel_write_count "${CMAKE_MATCH_1}")

string(
    REGEX MATCH
    "Bus write trace complete: ([0-9]+) selected writes"
    emulated_bus_line
    "${emulated_output}"
)
if(emulated_bus_line STREQUAL "" OR CMAKE_MATCH_1 LESS 1500000)
    message(
        FATAL_ERROR
        "The emulated route did not trace the VDC base ports.\n"
        "${emulated_output}"
    )
endif()
set(panel_bus_count "${CMAKE_MATCH_1}")

foreach(artifact IN ITEMS census write bus)
    if(artifact STREQUAL "census")
        set(emulated_path "${emulated_census}")
        set(replaced_path "${replaced_census}")
    elseif(artifact STREQUAL "write")
        set(emulated_path "${emulated_writes}")
        set(replaced_path "${replaced_writes}")
    elseif(artifact STREQUAL "bus")
        set(emulated_path "${emulated_bus}")
        set(replaced_path "${replaced_bus}")
    else()
        set(emulated_path "${emulated_calls}")
        set(replaced_path "${replaced_calls}")
    endif()
    file(SHA256 "${emulated_path}" emulated_sha256)
    file(SHA256 "${replaced_path}" replaced_sha256)
    if(NOT emulated_sha256 STREQUAL replaced_sha256)
        message(
            FATAL_ERROR
            "The replaced score-panel family's ${artifact} differs from the "
            "emulated route.\nEmulated: ${emulated_sha256}\n"
            "Replaced: ${replaced_sha256}\n"
            "Re-record both and diff them. A differing output_status on calls "
            "that write nothing is a mis-modelled flag -- `BIT #$01` clears N "
            "and V in this core; a differing return_instruction_pc is a gate "
            "read from the wrong address; a shifted bus trace is the dirty bit "
            "consumed on the wrong call."
        )
    endif()
endforeach()

file(SHA256 "${emulated_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(
        FATAL_ERROR
        "Semantic score-panel census artifact changed.\n"
        "Expected: ${expected_census_sha256}\nActual:   ${census_sha256}\n"
    )
endif()

file(REMOVE
    "${emulated_census}" "${emulated_writes}" "${emulated_bus}"
    "${emulated_calls}" "${emulated_calls}.writes.csv"
    "${replaced_census}" "${replaced_writes}" "${replaced_bus}"
    "${replaced_calls}" "${replaced_calls}.writes.csv")

message(
    STATUS
    "The score panel's ornament handoff and steppers run natively on all "
    "1/669/670 calls, its redraw poll on all 2,649 calls, and its redraw "
    "head/digit/zero/ball-count transactions on all 34/105/133/34 calls; "
    "their composed glyph work plus the two independently entered glyph calls "
    "have the same arms, registers, flags and timing as "
    "assembly; the route's "
    "${panel_write_count} panel writes and ${panel_bus_count} VDC port writes "
    "land on the same cycles."
)
