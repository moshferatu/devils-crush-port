# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported palette-shadow configuration: ${CONFIG}")
endif()

# The palette shadow buffer at $27B9-$2BB8. Gameplay code never touches the VCE
# directly: it keeps thirty-two 32-byte blocks of packed nine-bit GRB here,
# marks the changed ones in $2CCC-$2CEB, and 00:EB30 pushes the dirty blocks out
# once a frame.
#
# Five routines share one walk, and none of them calls anything, so each is a
# straight transcript to its own RTS.
#
#   00:E93C  the sixteen-bit compare the fade drivers branch on
#   00:E9B6  the block interpolator, one crossfade step over sixteen colours
#   00:EAA7  the fade-to-black stepper, one step over all thirty-two blocks
#   00:EB30  the upload: the colour-table address through $0402/$0403 and
#            sixteen colours per dirty block through $0404/$0405
#   00:EBA5  the block stager, a 32-byte copy plus the dirty flags
#
# 00:EB30 is now admitted on every one of its 3,599 calls, and the twenty-nine
# that used to be refused are the reason 00:EB4D is a seventh registered
# address. Those calls take the whole-palette arm, where a single TIA moves all
# 1,024 bytes to the data ports -- 1,024 writes against a replacement channel
# that carries 512, and a block transfer has no interior boundary for a chunk
# to stop at. So the arm splits at the instruction rather than declining at the
# routine: 00:EB30 walks the flag tests and both colour-table stores and stops
# in front of the transfer, the core executes it, and 00:EB4D clears $2BB9 and
# $2CCB on the other side. The counts below are the check that the split lands
# where it should -- 3,599 and 29, and 29 at 00:EB4D is one entry per arm.
#
# This buys admission, not cycles: the TIA is 625,095 of the arm's 627,618
# master cycles, and the six instructions now transcribed around it are 2,523.
# That is the honest shape of the row, and worth stating because the reverse
# was true of the interrupt dispatcher, where the one instruction the contract
# could not express was 17% of what the split moved.
#
# 00:EAA7 is the one that does not fit in a transcript either, and unlike the
# TIA arm it is only too long: 512 colours a call, 5,300 work-RAM writes and
# 205,012 master cycles. It chunks. 00:EAB0, the head of its colour loop, is a
# sixth registered address; a transcript stops in front of the JMP $EAB0 that
# closes the loop once a channel is nearly full and the core takes the jump and
# calls back. Eight calls become 101 entries, so the two counts below are the
# check that the chunk boundary has not moved -- 93 at 00:EAB0 is thirteen
# chunks a call, and a different budget would change it without changing any
# artifact hash.
#
# The two fade drivers above the shadow buffer are still deliberately absent,
# and the reason has been corrected. Both 00:E94D and 00:EA77 call the frame-
# sync spin at 00:E321, and the note here used to say that made a transcript
# span twenty-one frames. It does not: a walk stops in front of the JSR the way
# 00:EB30 now stops in front of the TIA, and the drivers would chunk on their
# passes. They are left alone because this route enters 00:E94D once and
# 00:EA77 eight times, for 138,582 and 1,455 master cycles. A single sample
# cannot show a transcript is state-generic rather than ordinal, and that bar
# is the point of every entry point above.
#
# Five work-RAM ranges and the VCE register file scope this, and all three
# artifacts are compared against what a pure-assembly run of the same route
# produces, so the writes match in value, address, order and master-cycle
# offset, not merely in end state. The VCE range is what pins 00:EB30's timing:
# a store to $0400-$0405 costs the ordinary absolute fifteen master cycles plus
# the cycle the VCE steals, and a transcript that misprices it drifts three
# cycles per colour. The stack range $21E0-$21FF is here for the same reason it
# is in the sequencer's regression: nothing in this slice pushes, so every stack
# write in the trace belongs to surrounding code, and any cycle these
# transcripts misprice moves those writes.
#
# All three hashes are the ones the four-entry-point version of this test
# asserted, and that is the point of the slice's verification rather than an
# oversight. The whole-palette split is checked hardest by the bus trace: the
# two STZ stores it moved into a transcript go to $0402 and $0403, which are
# inside the traced VCE range, so their values, order and master-cycle offsets
# are pinned against assembly rather than merely their effect on the screen. The ranges above already covered every byte 00:EAA7 touches --
# $2C, $2F/$30, $35/$36 and $3C-$3E in the zero-page range, and its stores
# through ($2F),Y in the shadow buffer -- so the artifact carries 42,770 writes
# from inside the routine. They were byte-identical to assembly before it was
# ported and they still are.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "5b69f71323f7e971c4ef032033a376029a359ee3564dcfe0b99e9474a7ed2bc2"
)
set(
    expected_bus_sha256
    "699e8bc20ed4397d54409a2c142082e7f93dbfa60be9033b99ccc02f35456823"
)

set(write_output "${OUTPUT}.writes.csv")
set(bus_output "${OUTPUT}.bus.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "00:E93C@all"
        --replace-hook
        "00:E9B6@all"
        --replace-hook
        "00:EAA7@all"
        --replace-hook
        "00:EAB0@all"
        --replace-hook
        "00:EB30@all"
        --replace-hook
        "00:EB4D@all"
        --replace-hook
        "00:EBA5@all"
        --work-ram-write-range
        "202B-2042"
        --work-ram-write-range
        "2047-204A"
        --work-ram-write-range
        "21E0-21FF"
        --work-ram-write-range
        "27B9-2BB8"
        --work-ram-write-range
        "2CCB-2CEB"
        --work-ram-write-trace
        "${write_output}"
        --bus-write-range
        "0400-0405"
        --bus-write-trace
        "${bus_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic palette-shadow recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:E93C@all entries: 8969 of 8969"
    "Native replacement 00:E9B6@all entries: 672 of 672"
    "Native replacement 00:EAA7@all entries: 8 of 8"
    "Native replacement 00:EAB0@all entries: 93 of 93"
    # 3,561 of 3,590 before replacements could nest: nine of this group's own
    # calls arrive inside a suspended sibling, and the core would not install a
    # second transcript. Then 3,570 of 3,599, the twenty-nine whole-palette
    # calls being the remainder. Now every arrival is transcribed.
    "Native replacement 00:EB30@all entries: 3599 of 3599"
    "Native replacement 00:EB4D@all entries: 29 of 29"
    "Native replacement 00:EBA5@all entries: 1121 of 1121"
    "Work-RAM write trace complete: 7369685 selected writes"
    "Bus write trace complete: 115688 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic palette-shadow output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census write bus)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${bus_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Semantic palette-shadow ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic palette shadow buffer runs its block interpolator, block stager, "
    "sixteen-bit compare, fade-to-black stepper and VCE upload natively across "
    "14,491 admitted entries as instruction-boundary transcripts -- the "
    "stepper in thirteen chunks a call, the upload's whole-palette arm split "
    "either side of its TIA -- with the shadow buffer, its dirty "
    "flags, the zero-page scratch, the stack, every VCE register write and the "
    "whole state census identical to assembly."
)
