# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported collision-scan configuration: ${CONFIG}")
endif()

# The native sweep must leave the emulated machine bit-identical to assembly,
# so both artifacts are the unmodified assembly-only baselines. The sweep now
# composes the vertical scanner at 01:CC1A instead of stopping in front of the
# call, so the ranges cover the whole composed transaction and not just the
# sweep's own counters: the scanner's difference scratch at $202D-$2030 and its
# magnitude pair at $2042-$2043, the sweep's countdown at $2060 and slot cursor
# at $2064, and the stack page, where the transcribed JSR and RTS push and pull
# the return address. The sweep is an instruction-boundary transcript and
# crosses interrupt horizons, so that is the assertion that matters: the
# interrupts still land where assembly put them.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "fedf42e41fe23107ee48268173b872ad84cf7159ea4c7ea8106c5341dfbbfbbb"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "01:CB48@all"
        --work-ram-write-range
        "202D-2030"
        --work-ram-write-range
        "2042-2043"
        --work-ram-write-range
        "2060-2064"
        --work-ram-write-range
        "2100-21FF"
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
        "Semantic collision-scan recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:CB48@all entries: 87968 of 87968"
    "Work-RAM write trace complete: 7147218 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic collision-scan output omitted '${expected_line}'.\n"
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
            "Semantic collision-scan ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic CB48 collision sweep rejects slots and composes the vertical "
    "scanner at 01:CC1A natively across all 87,968 sweep-head entries as an "
    "instruction-boundary transcript, with the scanner scratch, the "
    "$2060/$2064 counter writes, the stack page, and the whole state census "
    "identical to assembly."
)
