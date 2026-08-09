# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported VBlank-handler configuration: ${CONFIG}")
endif()

# The VDC interrupt handler's body. 00:E058 takes the interrupt, reads the VDC
# status register at $0000 into $01 and dispatches on it. That dispatcher is not
# portable -- its whole decision comes from a device register a replacement
# callback cannot read -- but nothing downstream of the read needs the device
# again, so the bodies are.
#
# Six entry points share one walk: the two arms the dispatcher jumps to, the two
# addresses the VBlank arm's calls come back to, and the secondary handler's
# prologue and epilogue.
#
#   00:E0EB  VBlank prologue: program VDC registers $08/$07/$05 from the RAM
#            shadows, then stop at the JSR $EB30
#   00:E117  after the pad read: bump the frame counter at $00 and either take
#            the bank wrapper at 00:ECC6 or set $DA and fall into the epilogue
#   00:E123  after that wrapper: restore MPR6 and join the epilogue
#   00:E136  the non-VBlank arm: reprogram the split registers from $0B/$0C on
#            raster modes 0 and 2, then bump the 16-bit tick at $222A
#   00:E179  the secondary handler's prologue, down to its JSR $ECC6
#   00:E183  its epilogue: restore MPR6, rewrite $0000 twice and unwind
#
# All six are admitted on every call.
#
# Each walk stops one instruction short of its RTI, in front of the PLP that
# restores the interrupted status byte, and that is the point this regression
# exists to hold. The handler does not run masked throughout: it is entered with
# I set, but bank 45 ends three critical sections with an unconditional CLI
# instead of restoring the caller's mask, so the music tick below 00:ECC6 hands
# the handler back with interrupts enabled. 78 of the route's interrupt entries
# land inside these six walks. PLP is the only instruction here that
# changes I, and the core samples the IRQ at LastCycle -- before PLP pops -- so a
# transcript that covered it would sample with the restored flag and swallow an
# interrupt assembly services between the PLP and the RTI. Nothing in the state
# census alone would necessarily show that; the stack page does, because a
# swallowed interrupt moves where its handler pushes.
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
    "89b2547bd06104b8fdf32da5b3a8a469d62851a8f7d75c9dda134ff64a21e34e"
)
set(
    expected_bus_sha256
    "1221e1687b256ca6621bd2088de29a1ceed4c0ad0d05d7c75d9107df825d7005"
)

set(write_output "${OUTPUT}.writes.csv")
set(bus_output "${OUTPUT}.bus.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "00:E0EB@all"
        --replace-hook
        "00:E117@all"
        --replace-hook
        "00:E123@all"
        --replace-hook
        "00:E136@all"
        --replace-hook
        "00:E179@all"
        --replace-hook
        "00:E183@all"
        --work-ram-write-range
        "2000-2002"
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
    "Native replacement 00:E0EB@all entries: 2748 of 2748"
    "Native replacement 00:E117@all entries: 3599 of 3599"
    "Native replacement 00:E123@all entries: 3212 of 3212"
    "Native replacement 00:E136@all entries: 3599 of 3599"
    "Native replacement 00:E179@all entries: 387 of 387"
    "Native replacement 00:E183@all entries: 387 of 387"
    "Work-RAM write trace complete: 4989037 selected writes"
    "Bus write trace complete: 2036234 selected writes"
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
    "Semantic VDC interrupt handler runs all six entry points natively across "
    "13,932 admitted entries as instruction-boundary transcripts, with the "
    "frame counter, the raster tick, the zero-page shadows, the whole stack "
    "page, every VDC register write and the state census identical to assembly."
)
