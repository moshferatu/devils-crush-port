# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported VRAM-upload configuration: ${CONFIG}")
endif()

# The graphics driver's four port-streaming loops. All of 00:E3C0-E47C points a
# VDC or VCE write pointer at a base address and then pushes words through the
# data ports until a counter runs out; nothing in the region reads a device
# register, so unlike the interrupt dispatcher above it the whole region is
# expressible.
#
#   00:E3C0  clear 512 VRAM words from $7800
#   00:E3FC  clear the 1,024-entry VCE colour table
#   00:E41F  the generic word-stream decoder -- $01-$7F literal words, $81-$FF a
#            repeated word, $00 terminates
#   00:FA40  the one-shot 26-row title/setup tile-and-attribute uploader
#
# This is the first chunked family in the port. One decoder call is 167,000
# master cycles, 3,400 port writes and 12,400 instructions, against a channel of
# 512 writes and 2,048 checkpoints, so no single transcript can hold one. Each
# routine therefore has two entry points -- its head, and the head of the loop
# its transcripts stop inside -- and a chunk ends in front of the backward
# branch that closes the loop. The core executes that branch, lands on the loop
# head and calls the replacement again. The 41 decoder calls become 41 head
# transcripts and 326 continuations, and the counts below are what holds that
# split: a chunk that failed to make progress would loop forever, and one that
# overran a channel would decline.
#
# The decoder's zero page is shared. $2F/$30 is the game's general-purpose
# source pointer, written from some 340 sites across eight banks, so a
# transcript reading it is only sound if nothing writes it underneath. That was
# measured before the port rather than assumed: bracketing each of the 41 calls
# between its header's $06 store at 00:E422 and its last data-port write, all
# 23,300 writes to $2D-$30/$37/$38 inside those windows come from the decoder
# itself and none from interrupt context. $2D-$30, $37 and $38 are therefore in
# scope here, together with the whole stack page.
#
# Both device register files are censused directly with --bus-write-range. A
# store to $0000-$0003 or $0402-$0405 costs the ordinary absolute fifteen master
# cycles plus the cycle the device steals; the VDC additionally wait-states the
# CPU while it reaches VRAM, which lands on the real timestamp inside the
# declared offset and is not the transcript's to predict. All three artifacts
# are compared against a pure-assembly run of the same route, so the writes
# match in value, address, order and master-cycle offset rather than merely in
# end state.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "8806527d038f5ff9e152a59bc3e97b93a68b9cfb15b42a93a8ddda6860a90d88"
)
set(
    expected_bus_sha256
    "01dc69cb955be192d0f63bb334e4a475fc5454ae5bd0ce6ba8aefae6394313d2"
)

set(write_output "${OUTPUT}.writes.csv")
set(bus_output "${OUTPUT}.bus.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "00:E3C0@all"
        --replace-hook
        "00:E3D7@all"
        --replace-hook
        "00:E3FC@all"
        --replace-hook
        "00:E403@all"
        --replace-hook
        "00:E41F@all"
        --replace-hook
        "00:E439@all"
        --replace-hook
        "00:FA40@all"
        --replace-hook
        "00:FA6A@all"
        --replace-hook
        "00:FA6E@all"
        --work-ram-write-range
        "2006-2006"
        --work-ram-write-range
        "202D-2030"
        --work-ram-write-range
        "2037-2038"
        --work-ram-write-range
        "2100-21FF"
        --work-ram-write-trace
        "${write_output}"
        --bus-write-range
        "0000-0003"
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
        "Semantic VRAM-upload recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:E3C0@all entries: 2 of 2"
    "Native replacement 00:E3D7@all entries: 4 of 4"
    "Native replacement 00:E3FC@all entries: 4 of 4"
    "Native replacement 00:E403@all entries: 16 of 16"
    "Native replacement 00:E41F@all entries: 41 of 41"
    "Native replacement 00:E439@all entries: 326 of 326"
    "Native replacement 00:FA40@all entries: 26 of 26"
    "Native replacement 00:FA6A@all entries: 416 of 416"
    "Native replacement 00:FA6E@all entries: 416 of 416"
    "Work-RAM write trace complete: 6036242 selected writes"
    "Bus write trace complete: 2151922 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic VRAM-upload output omitted '${expected_line}'.\n"
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
            "Semantic VRAM-upload ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic VRAM and VCE upload paths run all nine entry points natively "
    "across 1,251 admitted entries as chunked instruction-boundary transcripts, "
    "with the decoder's shared zero page, the whole stack page, every VDC and "
    "VCE register write and the state census identical to assembly."
)
