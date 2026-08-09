# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported actor-loop-linkage configuration: ${CONFIG}")
endif()

# The three instructions that hold the main actor loop together: the BSR into
# the dispatch half, the JSR into the queue filer, and the queue filer's RTS.
# Everything they connect is already native, so these carry no game state at
# all — their entire output is stack traffic. That is exactly what this
# regression asserts. The scoped range is the pair of stack bytes the loop
# writes at its usual $FD stack pointer, so a mismodelled push order, pushed
# value, or push timestamp shows up directly rather than only as downstream
# divergence. Mednafen pushes before it charges a cycle for either JSR or BSR,
# so both bytes carry the instruction's start offset; RTS pops, so it writes
# nothing and is proven by the census alone.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "4d427c7364d20184fafd43efde140f13294e5084550414b617e6bf35f0f77ddb"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "01:C51E@all"
        --replace-hook
        "01:C520@all"
        --replace-hook
        "01:C654@all"
        --work-ram-write-range
        "21FC-21FD"
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
        "Semantic actor-loop-linkage recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

# One arrival each per dispatched slot: the loop only reaches the BSR when the
# walk at `01:C510` hands a live slot over, and the JSR and its return follow
# every one of those.
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:C51E@all entries: 171734 of 171734"
    "Native replacement 01:C520@all entries: 171734 of 171734"
    "Native replacement 01:C654@all entries: 171734 of 171734"
    "Work-RAM write trace complete: 1068240 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic actor-loop-linkage output omitted '${expected_line}'.\n"
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
            "Semantic actor-loop-linkage ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic C51E/C520/C654 actor-loop linkage performs every one of its "
    "515,202 calls and returns natively as instruction-boundary transcripts, "
    "with the stack bytes it pushes and the whole state census identical to "
    "assembly."
)
