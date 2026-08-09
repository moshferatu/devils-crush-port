# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported sound-driver configuration: ${CONFIG}")
endif()

# The timer interrupt handler, which is the game's sound driver. It is a flat
# state machine with no subroutine calls: three PSG channels walked at X = 4,
# 2 and 0, each streaming five-bit DDA samples out of a sequence bank mapped
# into MPR6 and pushed through the PSG waveform port at $0806. The walk runs
# from the interrupt vector's landing at `00:FC00` to whichever of the three
# exits control reaches, and leaves the epilogue emulated: from FC32's CLI the
# handler is interruptible, and its epilogue reads $DA, the rerun request a
# preempting handler sets. A transcript is computed once from one snapshot and
# cannot observe that write, so the handler would never take its JMP $FC03
# chain exit into the sequencer.
#
# Two censuses scope this. The work-RAM range covers the driver's own state:
# the per-channel records at $20D9-$20FA and the track pointers at
# $2E10-$2E1F. The bus range covers what the driver is actually for — every
# write to the PSG register file, the timer, and the interrupt acknowledge.
# The census would catch a wrong value reaching $0806 on its own, since it
# hashes the audio as well as work RAM; the bus trace localises it to a
# single write, value, cycle and instruction instead.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "c4195b4eed0e9953476b5b18fdbef50ca58eb980f28b084a48eaacd09fe0af52"
)
set(
    expected_bus_sha256
    "fec8e78f16c0b6ffe3cd2de88c93c5bc19127aee368a246bd8cbff4e3ca053d2"
)

set(write_output "${OUTPUT}.writes.csv")
set(bus_output "${OUTPUT}.bus.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "00:FC00@all"
        --work-ram-write-range
        "20D9-20FA"
        --work-ram-write-range
        "2E10-2E1F"
        --work-ram-write-trace
        "${write_output}"
        --bus-write-range
        "0800-0809"
        --bus-write-range
        "0C00-0C01"
        --bus-write-range
        "1402-1403"
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
        "Semantic sound-driver recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

# Every entry is transcribed, including the reentrant ones: a timer interrupt
# that arrives while the handler is already running bails out through FC8C
# without touching a channel, which is the walk's third exit.
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:FC00@all entries: 254434 of 254434"
    "Work-RAM write trace complete: 1604764 selected writes"
    "Bus write trace complete: 954241 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic sound-driver output omitted '${expected_line}'.\n"
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
            "Semantic sound-driver ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic FC00 timer sound driver streams all three PSG channels "
    "natively across 254,434 of 254,434 entries as instruction-boundary "
    "transcripts, with its channel records, all 954,241 PSG, timer and "
    "interrupt-acknowledge writes, and the whole state census identical "
    "to assembly."
)
