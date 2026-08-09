# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "Unsupported actor-scan-advance configuration: ${CONFIG}"
    )
endif()

# The native tail must leave the emulated machine bit-identical to assembly,
# so both artifacts are the unmodified assembly-only baselines. The write range
# is the loop counter at $204D, which is the whole of this tail's output, and
# its trace is the same 450,469 writes the `01:C510` walk asserts — the two
# routines cover the same increment from opposite sides. The tail is an
# instruction-boundary transcript and crosses interrupt horizons, so that is
# the assertion that matters: the interrupts still land where assembly put
# them.
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
        "01:C523@all"
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
        "Semantic actor-scan-advance recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

# Hooked on its own the tail sees every iteration of the loop, not only the
# ones a dispatch hands back; deployed alongside `01:C510` the walk absorbs
# the rest inside its own transaction.
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:C523@all entries: 446976 of 446976"
    "Work-RAM write trace complete: 450469 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic actor-scan-advance output omitted '${expected_line}'.\n"
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
            "Semantic actor-scan-advance ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic C523 actor-loop tail advances the slot counter natively across "
    "all 446,976 entries as an instruction-boundary transcript, with the "
    "$204D counter writes and the whole state census identical to assembly."
)
