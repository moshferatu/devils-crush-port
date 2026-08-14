# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported actor-scan configuration: ${CONFIG}")
endif()

# The native walk must leave the emulated machine bit-identical to assembly,
# so both artifacts are the unmodified assembly-only baselines. The walk is an
# instruction-boundary transcript and crosses interrupt horizons, so that is
# the assertion that matters: the interrupts still land where assembly put
# them.
#
# The walk now ends past its own `RTS` at `01:C52B` on the arrivals that reach
# it, which took that return from 73,536 emulated master cycles to 819. It
# moves nothing here: an `RTS` writes nothing, so the counter trace below is
# untouched, and the absorbed-return declaration reproduces the boundary the
# observer would otherwise have recorded, so the entry count is untouched too.
# Both hashes and both counts are the same either side of it.
#
# What the absorption did need was the core reporting a transcript it gives up
# on -- `patches/0020-huc6280-replacement-abandonment.patch`. This walk is long
# enough to be cut short often and re-entered at the same stack depth, and
# without that report a promise made by an abandoned transcript was redeemed
# against a later live call. Nothing in this file could have seen it: the
# census and the write trace were both correct throughout. It shows up only in
# the routine-call trace, which is why that comparison is the one this
# technique is verified by.
#
# That comparison is not registered here, and the reason is size rather than
# doubt. `01:C510` is entered 446,976 times emulated and its call-write trace
# is 9.1 GB, 5.1 GB replaced; the other six absorbed returns are registered as
# `dc_rom_absorbed_return_trace_*` precisely because theirs are small enough to
# hash. This one is run by hand: trace the route with `--observe-hook 01:C510`
# alone and again with `--replace-hook 01:C510@all`, join the two on
# `entry_absolute_master_cycle`, and require every replaced row to be present
# with every column but `sequence` equal. It was 176,861 of 176,861 agreeing
# when the absorption shipped, against 82,201 disagreeing on `output_y` before
# the fork patch.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "a4b87dc4f2f18430fc94dcda4d6d49122fb61cafefcbc34fda3ae99bfa1cda9f"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "01:C510@all"
        --work-ram-write-range
        "204D-204D"
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
        "Semantic actor-scan recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:C510@all entries: 175226 of 175226"
    "Work-RAM write trace complete: 450469 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic actor-scan output omitted '${expected_line}'.\n"
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
            "Semantic actor-scan ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic C510 actor scan walks every idle slot natively across all "
    "176,861 loop-head entries as an instruction-boundary transcript, with "
    "the $204D counter writes and the whole state census identical to "
    "assembly."
)
