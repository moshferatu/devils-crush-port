# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported VBlank-handler configuration: ${CONFIG}")
endif()

# The VDC interrupt handler. 00:E058 takes the interrupt, reads the VDC status
# register at $0000 into $01 and dispatches on it.
#
# Exactly one instruction of that is unportable, and the whole dispatcher was
# recorded as blocked on its account for four slices. `LDA $0000` cannot be
# transcribed because the read *is* the interrupt acknowledge -- it clears every
# latched status source and is the only way the VDC's interrupt line falls --
# and the replacement contract is a write channel, so a transcript that
# predicted the byte and skipped the read would hand the core back an interrupt
# that had never been taken. That is a stronger reason than the one recorded
# before, which was that a callback cannot see a device register: the entry
# could have carried a peek, the way it already carries the frame's pad state,
# and it still would not have helped.
#
# The routine was the wrong unit. The four register saves above the read touch
# nothing but the stack page, and below the read the byte the dispatcher
# branches on is in the accumulator, so the device is never needed again. The
# head walks to the read and stops; the core executes it, at the right cycle,
# with its acknowledge; the tail picks the arm up from the accumulator.
#
# Eleven entry points share one walk: the dispatcher's two halves, the two arms
# it hands off to, the two addresses the VBlank arm's calls come back to, the
# secondary handler's prologue and epilogue, the pad reader one of those calls
# goes to, and the two interrupt returns.
#
#   00:E058  the dispatcher's head: push P, A, X and Y and stop in front of the
#            LDA $0000 that only the core can run
#   00:E05F  its tail: stash the status byte at $01, test the VBlank bit and
#            pick the arm from the raster mode at $54
#   00:E0EB  VBlank prologue: program VDC registers $08/$07/$05 from the RAM
#            shadows, then stop at the JSR $EB30
#   00:E117  after the pad read: bump the frame counter at $00 and either take
#            the bank wrapper at 00:ECC6 or set $DA and fall into the epilogue
#   00:E123  after that wrapper: restore MPR6 and join the epilogue
#   00:E136  the non-VBlank arm: reprogram the split registers from $0B/$0C on
#            raster modes 0 and 2, then bump the 16-bit tick at $222A
#   00:E179  the secondary handler's prologue, down to its JSR $ECC6
#   00:E183  its epilogue: restore MPR6, rewrite $0000 twice and unwind
#   00:E195  the pad reader: poll all five ports and publish the held state at
#            $2013,Y, the previous frame's at $200E,Y and the edges at $2018,Y
#   00:E135  the shared epilogue's RTI, and 00:E194 the secondary handler's:
#            one instruction each, three bytes off the stack
#
# All eleven are admitted on every call. The dispatcher's two halves take 7,198
# entries each, one per interrupt: 3,599 VBlank and 3,599 raster. 00:E135 takes
# the same 7,198 because every interrupt the dispatcher takes leaves through it,
# whichever arm it went down, and 00:E194 takes the secondary handler's 387.
#
# `00:E195` is the one routine in the port that reads a device rather than only
# writing one, and it was left out for four slices on the rule that a
# replacement callback cannot see a device register. The rule is about the
# callback, not about the value. What the joypad answers is a function of two
# things the walk does know: the pad state for the frame, which the entry now
# carries because it is the same object the core answers from, and the SEL and
# CLR latch lines, which this routine drives itself through $1000. So the walk
# models the port instead of reading it -- $B0 over the selected pad's four
# lines, active low, with the multitap's cursor stepped by the same rule
# MultiTapWrite uses -- and the four writes to $1000 go out on the write
# channel so the port is left where assembly would have left it.
#
# That is what the $1000 bus range and the $200E-$201C work-RAM range below are
# for. The port writes pin the latch sequence and the pad bytes pin what the
# model computed, both against assembly, for all 3,599 calls.
#
# Each walk stops in front of the PLP that restores the interrupted status byte,
# and that is the point this regression exists to hold. The handler does not run
# masked throughout: it is entered with I set, but bank 45 ends three critical
# sections with an unconditional CLI instead of restoring the caller's mask, so
# the music tick below 00:ECC6 hands the handler back with interrupts enabled.
# 78 of the route's interrupt entries land inside these walks. PLP is the only
# instruction here whose I flag and whose interrupt sample disagree, because the
# core samples the IRQ at LastCycle -- before PLP pops -- so a transcript that
# covered it would sample with the restored flag and swallow an interrupt
# assembly services between the PLP and the RTI. Nothing in the state census
# alone would necessarily show that; the stack page does, because a swallowed
# interrupt moves where its handler pushes.
#
# The RTI past it is a separate entry rather than more of those walks for
# exactly that reason, and it is expressible where the PLP is not because RTI
# restores P *before* LastCycle: the flag the return produced is the one the
# boundary is judged on, so one checkpoint states both truthfully. The same
# stack page proves it. Both returns read three bytes out of it and resume
# wherever the interrupt was taken, and a transcript that read the wrong bytes,
# priced the seven cycles wrong, or resumed in the wrong bank would move every
# push made afterwards.
#
# So the whole stack page is in scope, and the VDC's three base ports are
# censused directly with --bus-write-range: a store to $0000-$0003 costs the
# ordinary absolute fifteen master cycles plus the cycle the VDC steals, and a
# transcript that misprices it drifts three cycles per register write. All three
# artifacts are compared against what a pure-assembly run of the same route
# produces, so the writes match in value, address, order and master-cycle offset
# rather than merely in end state.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "69dc205455c9073c163bf02b846317d234522cadecc4eb9182e24a0495ca92cd"
)
set(
    expected_bus_sha256
    "adc3b5cca8ec8244b0cd31067a9b53e585fed631d390534e0c0fecf5617e1a70"
)

set(write_output "${OUTPUT}.writes.csv")
set(bus_output "${OUTPUT}.bus.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "00:E058@all"
        --replace-hook
        "00:E05F@all"
        --replace-hook
        "00:E0EB@all"
        --replace-hook
        "00:E117@all"
        --replace-hook
        "00:E123@all"
        --replace-hook
        "00:E135@all"
        --replace-hook
        "00:E136@all"
        --replace-hook
        "00:E179@all"
        --replace-hook
        "00:E183@all"
        --replace-hook
        "00:E194@all"
        --replace-hook
        "00:E195@all"
        --work-ram-write-range
        "2000-2002"
        --work-ram-write-range
        "200E-201C"
        --work-ram-write-range
        "20D9-20DA"
        --work-ram-write-range
        "2100-21FF"
        --work-ram-write-range
        "222A-222B"
        --work-ram-write-trace
        "${write_output}"
        --bus-write-range
        "0000-0003"
        --bus-write-range
        "1000-1000"
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
        "Semantic VBlank-handler recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:E058@all entries: 7198 of 7198"
    "Native replacement 00:E05F@all entries: 7198 of 7198"
    "Native replacement 00:E0EB@all entries: 2748 of 2748"
    "Native replacement 00:E117@all entries: 3599 of 3599"
    "Native replacement 00:E123@all entries: 3212 of 3212"
    "Native replacement 00:E135@all entries: 7198 of 7198"
    "Native replacement 00:E136@all entries: 3599 of 3599"
    "Native replacement 00:E179@all entries: 387 of 387"
    "Native replacement 00:E183@all entries: 387 of 387"
    "Native replacement 00:E194@all entries: 387 of 387"
    "Native replacement 00:E195@all entries: 3599 of 3599"
    "Work-RAM write trace complete: 5061032 selected writes"
    "Bus write trace complete: 2079422 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic VBlank-handler output omitted '${expected_line}'.\n"
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
            "Semantic VBlank-handler ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic VDC interrupt handler runs all nine entry points natively across "
    "31,927 admitted entries as instruction-boundary transcripts, with the "
    "frame counter, the raster tick, the zero-page shadows, the whole stack "
    "page, the five pads' held and newly-pressed state, every VDC register "
    "write, every joypad latch write and the state census identical to "
    "assembly."
)
