# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported tilemap-upload configuration: ${CONFIG}")
endif()

# The background tilemap upload, 00:F261-F4F1 -- the second half of the graphics
# driver's port work, and two routines rather than one:
#
#   00:F3E7  the row writer. Programs a VRAM address into the VDC and pushes 32
#            words through the data ports, each a tile index from an MPR-mapped
#            source with an attribute nibble from a work-RAM array folded in.
#   00:F261  the scroll driver. Decides whether the camera has crossed a tile
#            boundary, resolves the tile source through the already-native bank
#            helper at 00:E1FD, computes the addresses, calls the writer once or
#            twice, and folds the movement back into the camera.
#
# Nothing in the region reads a device register, so the whole row is
# expressible. Ten entry points: the two routine heads, the six return addresses
# of the calls the driver makes, and 00:F426/00:F42A, the two arms of the BBR at
# 00:F423.
#
# Those six calls are crossed rather than stopped in front of, and this
# regression is where that is proved. A crossing writes the two return bytes
# itself, at the offset the JSR writes them -- both before the opcode has
# charged a cycle, so both carry the instruction's start offset -- and charges
# the same twenty-one cycles the core would have. If that is right then nothing
# observable moves, and the whole stack page is already in scope below, so all
# three expected hashes here are the ones this file carried before the crossings
# existed. They are not re-recorded expectations. What the crossings buy is
# where each walk ends: four of them on 00:F3E7 in this same family, and
# 00:F2A8 and 00:F35C on 00:E1FD, the asset resolver's one-instruction head,
# which is an entry of its own as of the same slice. Ending on a registered
# address is the entire point -- the twenty-one cycles cost what stopping cost,
# and buy the core entering a transcript instead of interpreting into one.
#
# That BBR is the reason for the last two. Every other instruction here charges
# its cycles in one ADDCYC and one LastCycle, which is exactly what a checkpoint
# expresses. BBR is assembled from two of them, so the machine crosses it in
# four clock steps where a checkpoint crosses it in two, and the coarser step
# runs the video EventHandler at a later timestamp. That moved a VDC event,
# which moved a timer interrupt one instruction early four times in 3,600
# frames. Handing 00:F423 to the core costs 365,664 master cycles of the region.
# The 8,624/8,624 split below is what holds it: the BBR branches on the parity
# of the word index, so exactly half the 17,248 iterations take each arm.
#
# The zero page here is shared -- $2B-$30 is the general-purpose pointer area
# and $A2-$AA the camera block -- so the walk is only sound if nothing writes
# them underneath. Measured against exact call windows from a 00:F261 observer
# rather than inferred from the writes: across all 2,649 calls, 92,978 writes
# inside the windows are the routine's own, and the only others are 8,118
# interrupt-frame pushes below the live stack pointer and four stores to $20 --
# an address the region writes only below the 00:E1FD it now crosses into, and
# never reads. The ranges below are the routine's whole output plus the stack
# page, and the VDC register file is censused directly with --bus-write-range.
# All three artifacts are compared against a pure-assembly run of the same
# route, so the writes match in value, address, order and master-cycle offset
# rather than merely in end state.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "1d908fc7399cf2746c11d6c9c8afe9a69278d8c336a6f16de8ad10aaea4dd53e"
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
        "00:F261@all"
        --replace-hook
        "00:F2AB@all"
        --replace-hook
        "00:F301@all"
        --replace-hook
        "00:F330@all"
        --replace-hook
        "00:F35F@all"
        --replace-hook
        "00:F3B5@all"
        --replace-hook
        "00:F3E4@all"
        --replace-hook
        "00:F3E7@all"
        --replace-hook
        "00:F426@all"
        --replace-hook
        "00:F42A@all"
        --work-ram-write-range
        "2006-2006"
        --work-ram-write-range
        "200A-200C"
        --work-ram-write-range
        "2026-2027"
        --work-ram-write-range
        "202B-2030"
        --work-ram-write-range
        "2035-2036"
        --work-ram-write-range
        "207A-207A"
        --work-ram-write-range
        "20A2-20AA"
        --work-ram-write-range
        "2100-21FF"
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
        "Semantic tilemap-upload recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:F261@all entries: 2649 of 2649"
    "Native replacement 00:F2AB@all entries: 286 of 286"
    "Native replacement 00:F301@all entries: 286 of 286"
    "Native replacement 00:F330@all entries: 5 of 5"
    "Native replacement 00:F35F@all entries: 232 of 232"
    "Native replacement 00:F3B5@all entries: 232 of 232"
    "Native replacement 00:F3E4@all entries: 16 of 16"
    "Native replacement 00:F3E7@all entries: 539 of 539"
    "Native replacement 00:F426@all entries: 8624 of 8624"
    "Native replacement 00:F42A@all entries: 8624 of 8624"
    "Work-RAM write trace complete: 6977084 selected writes"
    "Bus write trace complete: 2036234 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic tilemap-upload output omitted '${expected_line}'.\n"
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
            "Semantic tilemap-upload ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic background tilemap upload runs all ten entry points natively "
    "across 21,493 admitted entries, with the scroll driver's shared zero "
    "page, the whole stack page, every VDC register write and the state "
    "census identical to assembly."
)
