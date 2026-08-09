# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported SAT-tail configuration: ${CONFIG}")
endif()

# This sweep touches no work RAM at all: its entire output is 32 zero words
# per iteration pushed through the VDC data ports, so there is no scoped write
# trace to take. The state census is the assertion instead, and it is a strong
# one — it hashes the rendered framebuffer every frame, and a satellite
# attribute table whose tail was not cleared, or was cleared at the wrong
# cycle, draws stale sprites. The sweep is an instruction-boundary transcript,
# which is also what makes the VDC's stolen wait cycle land inside the
# declared offset rather than being charged twice.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "08:40C3@all"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic SAT-tail recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

# 3,492 calls, but 4,334 arrivals: a run longer than the transcript's
# 48-iteration budget resumes at the loop head and is picked up again.
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 08:40C3@all entries: 4334 of 4334"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic SAT-tail output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

file(SHA256 "${OUTPUT}" actual_sha256)
if(NOT actual_sha256 STREQUAL expected_census_sha256)
    message(
        FATAL_ERROR
        "Semantic SAT-tail census artifact changed.\n"
        "Expected: ${expected_census_sha256}\n"
        "Actual:   ${actual_sha256}\n"
    )
endif()

message(
    STATUS
    "Semantic 40C3 SAT tail clear zeroes every unused attribute-table entry "
    "natively across all 4,334 loop-head entries as an instruction-boundary "
    "transcript, with the whole state census — including every frame's "
    "rendered framebuffer — identical to assembly."
)
