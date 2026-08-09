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
# Four entry points share one walk, and none of them calls anything, so each is
# a single straight transcript to its own RTS.
#
#   00:E93C  the sixteen-bit compare the fade drivers branch on
#   00:E9B6  the block interpolator, one crossfade step over sixteen colours
#   00:EB30  the upload: the colour-table address through $0402/$0403 and
#            sixteen colours per dirty block through $0404/$0405
#   00:EBA5  the block stager, a 32-byte copy plus the dirty flags
#
# 00:EB30 is admitted on every call but twenty-nine. Those take its whole-
# palette arm, where a single TIA moves all 1,024 bytes to the data ports; that
# is 1,024 writes against a replacement channel that carries 512, so the walk
# declines in front of the first store rather than truncating the upload.
#
# The two fade drivers above the shadow buffer are deliberately absent. Both
# 00:E94D and 00:EA77 call the frame-sync spin at 00:E321, so a transcript for
# either would have to span twenty-one frames; and 00:EAA7, the fade-to-black
# stepper, is 205,012 master cycles per call, past the checkpoint cap however it
# is arranged.
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
        "00:EB30@all"
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
    "Native replacement 00:EB30@all entries: 3561 of 3590"
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
    "sixteen-bit compare and VCE upload natively across 14,323 admitted "
    "entries as instruction-boundary transcripts, with the shadow buffer, its "
    "dirty flags, the zero-page scratch, the stack, every VCE register write "
    "and the whole state census identical to assembly."
)
