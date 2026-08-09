# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported music-sequencer configuration: ${CONFIG}")
endif()

# The bank-45 music sequencer. It is a separate component from the timer sound
# driver at `00:FC00`: the driver streams DDA samples, the sequencer decides
# what they are, programming the PSG register file and maintaining the
# per-voice records in $2E00-$2FFF.
#
# Ten subroutine entry points share one walk. `48B4` is the command decoder
# `4883` and `48F6` call, so it is hooked in its own right but only sees the
# calls made from emulated code: the 25,896 calls made from inside a `4883`
# or `48F6` transcript are inlined into that transcript instead, which is why
# its admitted count is the small remainder. `457A`, the command interpreter,
# inlines four callees the same way -- the $42BA dispatch at `4569`, the
# balance programmer `4522`, the note-to-period conversion `46D7`, and the
# handlers themselves -- so none of those is a separate entry.
#
# Three entry points are admitted at less than 100%.
#
#   - `4142` reads the timer driver's channel bank bytes at $F5/$F7/$F9, and
#     `49F9` reaches $20F5,Y through its amplitude commit. Those bytes are
#     written by another thread of control, and a transcript reads them from a
#     snapshot taken before it runs, so the walk declines unless either the
#     interrupt mask is already set at entry -- `4142` brackets each voice in
#     PHP/SEI/PLP, so a masked entry stays masked throughout -- or the body
#     finishes before the next serviceable timer interrupt.
#   - `457A` declines its $82/$92 command, which pops four bytes and jumps to
#     $40EE, abandoning the routine's own frame; a leaf transcript that has to
#     stop in front of an RTS cannot express that. It also declines the
#     handlers this route never reaches, which are left untranscribed rather
#     than written blind.
#
# Four work-RAM ranges and two bus ranges scope this, and all six artifacts
# are compared against the artifacts a pure-assembly run of the same route
# produces, so the writes match in value, address, order and master-cycle
# offset, not merely in end state.
#
# Two of the ranges are load-bearing rather than incidental. The stack range
# $21E0-$21FF caught a JSR whose two return-address writes were credited 21
# cycles late, because the core pushes them *before* `ADDCYC(6)` while every
# other store in the walk lands on its instruction's final cycle; it then
# caught a PLP that published the caller's flags at its checkpoint, so an
# interrupt landing on that exact instruction pushed the pre-pop status. The
# driver range $20DD-$20FB covers the sample trigger at $4776, which writes
# the driver's whole per-channel zero page. Neither the state census nor the
# device-write trace saw either fault.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "a5701e57d182e9f5c50c46b5b3d4a2eddf3caa1d466a654301bb2bc336f7f21b"
)
set(
    expected_bus_sha256
    "b54d5c4ff41430623b0af9fe596d296aff7b84b4c5dd1ddc1af73eee26a6585e"
)

set(write_output "${OUTPUT}.writes.csv")
set(bus_output "${OUTPUT}.bus.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "45:4142@all"
        --replace-hook
        "45:4211@all"
        --replace-hook
        "45:4228@all"
        --replace-hook
        "45:457A@all"
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
        "20D2-20D8"
        --work-ram-write-range
        "20DD-20FB"
        --work-ram-write-range
        "21E0-21FF"
        --work-ram-write-range
        "2E00-2FFF"
        --work-ram-write-trace
        "${write_output}"
        --bus-write-range
        "0800-0809"
        --bus-write-range
        "0C00-0C01"
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
    "Native replacement 45:4142@all entries: 1792 of 3604"
    "Native replacement 45:4211@all entries: 7198 of 7198"
    "Native replacement 45:4228@all entries: 1142 of 1142"
    "Native replacement 45:457A@all entries: 2219 of 2299"
    "Native replacement 45:4883@all entries: 22664 of 22664"
    "Native replacement 45:48B4@all entries: 72 of 72"
    "Native replacement 45:48F6@all entries: 22664 of 22664"
    "Native replacement 45:49A2@all entries: 22664 of 22664"
    "Native replacement 45:49B6@all entries: 3604 of 3604"
    "Native replacement 45:49F9@all entries: 22622 of 22664"
    "Work-RAM write trace complete: 5458568 selected writes"
    "Bus write trace complete: 699251 selected writes"
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
    "Semantic bank-45 music sequencer runs its command interpreter, "
    "sample/envelope group, portamento stepper and PSG register programmer "
    "natively across 106,641 admitted entries as instruction-boundary "
    "transcripts, with its voice records, the zero-page command pointer, the "
    "timer driver's per-channel state, the stack, all 699,251 PSG and timer "
    "writes, and the whole state census identical to assembly."
)
