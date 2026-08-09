# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported camera-sweep configuration: ${CONFIG}")
endif()

# The loop around the already-native camera-activation test at `00:F4FC`, which
# decides for each of the 127 actor slots whether bit 7 of its attribute byte —
# the bit both `01:C510` and `08:407C` gate on — is set this frame. Like the SAT
# submission driver, it is one shared walk over the addresses control arrives at
# with no replacement active: the loop's initializer at `00:F4F2` and the three
# `RTS` instructions the child returns through.
#
# The scoped ranges are the sweep's product and its linkage: the attribute bytes
# at $3080-$30FE that the child writes, which prove the loop visits every slot
# in the right order at the right cycle, and the pair of stack bytes the loop's
# JSR pushes at its usual $FD stack pointer.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "f1325833b4a936b209f9a7b5779d4267fbc905d0669ffde8d0365ea137df4311"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "00:F4F2@all"
        --replace-hook
        "00:F523@all"
        --replace-hook
        "00:F545@all"
        --replace-hook
        "00:F54E@all"
        --work-ram-write-range
        "3080-30FE"
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
        "Semantic camera-sweep recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

# `00:F4FC` is also called from outside this loop fourteen times on the route.
# Those arrivals reach the same three `RTS` addresses with a different frame
# under the stack pointer, and the walk declines them rather than assume the
# caller — ten at `00:F523` and four at `00:F54E`.
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:F4F2@all entries: 519 of 519"
    "Native replacement 00:F523@all entries: 28813 of 28823"
    "Native replacement 00:F545@all entries: 8525 of 8525"
    "Native replacement 00:F54E@all entries: 28575 of 28579"
    "Work-RAM write trace complete: 1125975 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic camera-sweep output omitted '${expected_line}'.\n"
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
            "Semantic camera-sweep ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic F4F2/F523/F545/F54E camera sweep walks all 127 actor slots "
    "natively across 66,432 of 66,446 entries as instruction-boundary "
    "transcripts, with the attribute bytes it produces, the stack bytes it "
    "pushes, and the whole state census identical to assembly."
)
