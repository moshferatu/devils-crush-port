# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported collision-tail configuration: ${CONFIG}")
endif()

# The collision tail: the frame around the overlap scanner, which has been
# native since the twenty-sixth replacement. Five routines in physical bank 01,
# none of which reads a device register:
#
#   01:CD74  the playfield-tile probe, and the largest single piece of unported
#            logic left in phase 1 at 2.6M master cycles. It turns the ball
#            position into a tile coordinate, reads the tile id and its
#            attribute through two parallel maps, and tests the attribute's
#            eight-byte solidity mask against a one-bit column mask. It makes no
#            calls, so each of its 3,233 entries is a single transcript.
#   01:CA84  the ball/table gate: six 16-bit range tests through the compare
#            helper at 00:E93C decide which dispatch byte lands in $3D00,X.
#   01:CB2A  the sweep setup, which falls into the already-native sweep at
#            01:CB48, plus the tail at 01:CB74-01:CB92 that resolves the result.
#   01:CBAB  the single-slot probe and 01:CBCF the confirmed-contact
#            dispatcher, whose two PHAs and RTS are an ordinary tail jump for a
#            transcript: the pushes are transcribed and the core executes the
#            RTS.
#
# Nineteen entry points. Eleven are the addresses the core arrives at after a
# call this family makes, because a transcript stops in front of one; four are
# routine heads; 01:CD74 is the probe; and 01:CB81/01:CB8A are the two arms of
# the BBR at 01:CB7E, which the tilemap slice established a checkpoint cannot
# express -- BBR is LD_ZP wrapping JR, so the machine crosses it in four clock
# steps where a checkpoint crosses it in two, and the core tests next_event on
# those steps.
#
# The zero page here is shared with the rest of the game: $2D-$30 is the general
# pointer scratch, $26 a tile-id temporary, $64-$67 the sweep cursor and
# geometry, $78 the contact latch. Measured against exact windows from observers
# on the three top-level heads rather than inferred from the writes: across all
# 6,862 calls, every write to an address these walks read comes from bank 01
# game code or from the arithmetic helpers at 00:E56E-00:E847, and the split is
# clean rather than close. Those eighteen helper instructions land inside a
# window on 99.3% to 100% of their executions, which is what a callee looks
# like; every one of the three hundred other instructions that writes the same
# addresses anywhere on the route lands inside a window on exactly 0.00% of
# them. Nothing in the timer driver, the music sequencer or the VDC interrupt
# handler writes any of it.
#
# The ranges below are the family's whole output plus the stack page, compared
# against a pure-assembly run of the same route, so the writes match in value,
# address, order and master-cycle offset rather than merely in end state.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "99dbdf8397ebc3098692a948a0de881682059fb30b174cbf768bc4f783947bd1"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "01:CA84@all"
        --replace-hook
        "01:CAA1@all"
        --replace-hook
        "01:CAB6@all"
        --replace-hook
        "01:CAD3@all"
        --replace-hook
        "01:CAE0@all"
        --replace-hook
        "01:CAED@all"
        --replace-hook
        "01:CB04@all"
        --replace-hook
        "01:CB11@all"
        --replace-hook
        "01:CB2A@all"
        --replace-hook
        "01:CB74@all"
        --replace-hook
        "01:CB79@all"
        --replace-hook
        "01:CB81@all"
        --replace-hook
        "01:CB8A@all"
        --replace-hook
        "01:CBAB@all"
        --replace-hook
        "01:CBB6@all"
        --replace-hook
        "01:CBC2@all"
        --replace-hook
        "01:CBCA@all"
        --replace-hook
        "01:CBCF@all"
        --replace-hook
        "01:CD74@all"
        --work-ram-write-range
        "2026-2026"
        --work-ram-write-range
        "202D-2030"
        --work-ram-write-range
        "2053-2053"
        --work-ram-write-range
        "205E-2060"
        --work-ram-write-range
        "2064-2067"
        --work-ram-write-range
        "2077-2078"
        --work-ram-write-range
        "20B7-20B7"
        --work-ram-write-range
        "20BD-20BD"
        --work-ram-write-range
        "2100-21FF"
        --work-ram-write-range
        "2CFD-2CFD"
        --work-ram-write-range
        "3800-387F"
        --work-ram-write-range
        "3D00-3D7F"
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
        "Semantic collision-tail recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:CA84@all entries: 2649 of 2649"
    "Native replacement 01:CAA1@all entries: 2315 of 2315"
    "Native replacement 01:CAB6@all entries: 194 of 194"
    "Native replacement 01:CAD3@all entries: 194 of 194"
    "Native replacement 01:CAE0@all entries: 50 of 50"
    "Native replacement 01:CAED@all entries: 42 of 42"
    "Native replacement 01:CB04@all entries: 2121 of 2121"
    "Native replacement 01:CB11@all entries: 2121 of 2121"
    "Native replacement 01:CB2A@all entries: 3233 of 3233"
    "Native replacement 01:CB74@all entries: 2986 of 2986"
    "Native replacement 01:CB79@all entries: 3233 of 3233"
    "Native replacement 01:CB81@all entries: 299 of 299"
    "Native replacement 01:CB8A@all entries: 2934 of 2934"
    "Native replacement 01:CBAB@all entries: 980 of 980"
    "Native replacement 01:CBB6@all entries: 980 of 980"
    "Native replacement 01:CBC2@all entries: 980 of 980"
    "Native replacement 01:CBCA@all entries: 19 of 19"
    "Native replacement 01:CBCF@all entries: 270 of 270"
    "Native replacement 01:CD74@all entries: 3233 of 3233"
    "Work-RAM write trace complete: 6906417 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic collision-tail output omitted '${expected_line}'.\n"
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
            "Semantic collision-tail ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic collision tail runs all nineteen entry points natively across "
    "28,833 admitted entries, with the family's shared zero page, its whole "
    "output, the stack page and the state census identical to assembly."
)
