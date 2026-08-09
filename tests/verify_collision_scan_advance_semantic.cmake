# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "Unsupported collision-scan-advance configuration: ${CONFIG}"
    )
endif()

# The collision sweep's tail, and the counterpart of `01:C523` in the actor
# loop. The `01:CB48` walk absorbs this sequence for every slot it rejects,
# but a slot that reaches the vertical scan leaves the transaction at CC1A, so
# the branch and advance that follow are separate arrivals. The scoped ranges
# are the sweep's whole output: the slot cursor at $2064, the countdown at
# $2060, and the hit latch at $2077 that the carry-set branch sets. Both
# routines are instruction-boundary transcripts and cross interrupt horizons,
# so the write timestamps are part of the assertion.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "0cea1502c5905ab3a90ab8730ae33cd93300e13f956483eb296ad9acb87683cc"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "01:CB61@all"
        --replace-hook
        "01:CB6B@all"
        --work-ram-write-range
        "2060-2060"
        --work-ram-write-range
        "2064-2064"
        --work-ram-write-range
        "2077-2077"
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
        "Semantic collision-scan-advance recorder failed "
        "(${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

# Hooked on their own the advance sees every iteration of the sweep, not only
# the ones a vertical scan hands back; deployed alongside `01:CB48` the walk
# absorbs the rest inside its own transaction.
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:CB61@all entries: 84735 of 84735"
    "Native replacement 01:CB6B@all entries: 400892 of 400892"
    "Work-RAM write trace complete: 814294 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic collision-scan-advance output omitted "
            "'${expected_line}'.\n"
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
            "Semantic collision-scan-advance ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic CB61/CB6B collision-sweep tail resolves the scan result and "
    "advances the sweep natively across all 485,627 entries as "
    "instruction-boundary transcripts, with the slot, countdown, and latch "
    "writes and the whole state census identical to assembly."
)
