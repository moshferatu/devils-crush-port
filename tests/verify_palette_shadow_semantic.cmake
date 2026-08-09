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
# Three entry points share one walk, and all three are leaves -- no JSR, no
# device port, and nothing another thread of control writes -- so each is a
# single straight transcript and all three are admitted on every call.
#
#   00:E93C  the sixteen-bit compare the fade drivers branch on
#   00:E9B6  the block interpolator, one crossfade step over sixteen colours
#   00:EBA5  the block stager, a 32-byte copy plus the dirty flags
#
# The two drivers above them are deliberately absent. Both 00:E94D and 00:EA77
# call the frame-sync spin at 00:E321, so a transcript for either would have to
# span twenty-one frames; and 00:EB30, the upload itself, writes the VCE
# register file at $0400-$0405, which the replacement write channel does not
# carry yet.
#
# Four work-RAM ranges scope this, and both artifacts are compared against the
# artifacts a pure-assembly run of the same route produces, so the writes match
# in value, address, order and master-cycle offset, not merely in end state.
# The stack range $21E0-$21FF is here for the same reason it is in the
# sequencer's regression: nothing in this slice pushes, so every stack write in
# the trace belongs to surrounding code, and any cycle these transcripts
# misprice moves those writes.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "b9b124aa8388d09c01f499519b9a0fdb80a6843b87d176d8bc4e95cd65e61a86"
)

set(write_output "${OUTPUT}.writes.csv")

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
        "00:EBA5@all"
        --work-ram-write-range
        "202B-2042"
        --work-ram-write-range
        "21E0-21FF"
        --work-ram-write-range
        "27B9-2BB8"
        --work-ram-write-range
        "2CCB-2CEB"
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
        "Semantic palette-shadow recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:E93C@all entries: 8969 of 8969"
    "Native replacement 00:E9B6@all entries: 672 of 672"
    "Native replacement 00:EBA5@all entries: 1121 of 1121"
    "Work-RAM write trace complete: 7281171 selected writes"
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
            "Semantic palette-shadow ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic palette shadow buffer runs its block interpolator, block stager "
    "and sixteen-bit compare natively across 10,762 admitted entries as "
    "instruction-boundary transcripts, with the shadow buffer, its dirty "
    "flags, the zero-page scratch, the stack, and the whole state census "
    "identical to assembly."
)
