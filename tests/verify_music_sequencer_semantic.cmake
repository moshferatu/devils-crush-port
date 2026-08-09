# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported music-sequencer configuration: ${CONFIG}")
endif()

# The bank-45 music sequencer's sample/envelope group. The sequencer is a
# separate component from the timer sound driver at `00:FC00`: the driver
# streams DDA samples, the sequencer decides what they are, programming the
# PSG register file and maintaining the per-voice records in $2E00-$2FFF.
#
# Seven subroutine entry points share one walk. `48B4` is the command decoder
# `4883` and `48F6` call, so it is hooked in its own right but only sees the
# calls made from emulated code: the 25,896 calls made from inside a `4883`
# or `48F6` transcript are inlined into that transcript instead, which is why
# its admitted count is the small remainder.
#
# Three censuses scope this. The work-RAM ranges cover the zero-page command
# pointer at $20D2-$20D5 and every voice record; the bus range covers the PSG
# register file. All three are compared against the artifacts a pure-assembly
# run of the same route produces, so the writes match in value, address,
# order and master-cycle offset, not merely in end state.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "ed3813cdef9e1d9006dd9e0da915347798de8a527fb902ff60abbc69c252aae5"
)
set(
    expected_bus_sha256
    "befc335edb26986b65c7b35894b513ebe77a9f0524c01d05679601ad3c80f46e"
)

set(write_output "${OUTPUT}.writes.csv")
set(bus_output "${OUTPUT}.bus.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "45:4211@all"
        --replace-hook
        "45:4883@all"
        --replace-hook
        "45:48B4@all"
        --replace-hook
        "45:48F6@all"
        --replace-hook
        "45:49A2@all"
        --replace-hook
        "45:49B6@all"
        --replace-hook
        "45:49F9@all"
        --work-ram-write-range
        "20D2-20D5"
        --work-ram-write-range
        "2E00-2FFF"
        --work-ram-write-trace
        "${write_output}"
        --bus-write-range
        "0800-0809"
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
        "Semantic music-sequencer recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 45:4211@all entries: 7198 of 7198"
    "Native replacement 45:4883@all entries: 22664 of 22664"
    "Native replacement 45:48B4@all entries: 72 of 72"
    "Native replacement 45:48F6@all entries: 22664 of 22664"
    "Native replacement 45:49A2@all entries: 22664 of 22664"
    "Native replacement 45:49B6@all entries: 3604 of 3604"
    "Native replacement 45:49F9@all entries: 22664 of 22664"
    "Work-RAM write trace complete: 409323 selected writes"
    "Bus write trace complete: 698608 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic music-sequencer output omitted '${expected_line}'.\n"
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
            "Semantic music-sequencer ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic bank-45 music sequencer runs its sample/envelope group "
    "natively across all 101,530 admitted entries as instruction-boundary "
    "transcripts, with its voice records, the zero-page command pointer, all "
    "698,608 PSG writes and the whole state census identical to assembly."
)
