# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Tower bonus stage-selection regression is Release-only.")
endif()

set(expected_census_sha256 "ecbaa4f92a46e8de489a3ee6ca4c4a6ade374459b843f6c72d91dfcaac2c9c56")
set(expected_call_sha256 "dd0cc07c773dc126e66088d8ec48c846e48147502160336a65e2002ebfbf87ee")
set(expected_write_sha256 "05414c6b7d438487920f037faf41f8487337be101a1d81a3b173cb43af74aa8e")
set(expected_state_write_sha256 "715ad91439ae97c475b0eb437a17a160a30f2df2882e7f62dc94cd3765f3c8f1")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_write_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "03:60C3"
        --observe-hook
        "03:60F9"
        --observe-hook
        "03:612B"
        --observe-hook
        "03:6136"
        --observe-hook
        "01:CBCF"
        --replace-hook
        "03:60F9@all"
        --replace-hook
        "03:6136@all"
        --routine-call-trace
        "${call_output}"
        --work-ram-write-range
        "382D-382D"
        --work-ram-write-range
        "3180-3180"
        --work-ram-write-range
        "3200-3200"
        --work-ram-write-range
        "3300-3300"
        --work-ram-write-range
        "3380-3380"
        --work-ram-write-trace
        "${state_write_output}"
        --work-ram-patch
        "3352:2CFC:01"
        --work-ram-patch
        "3352:2D4D:01"
        --work-ram-patch
        "3353:2CFC:01"
        --work-ram-patch
        "3353:2D4D:01"
        --work-ram-patch
        "3354:3100:00"
        --work-ram-patch
        "3354:3180:90"
        --work-ram-patch
        "3354:3200:00"
        --work-ram-patch
        "3354:3280:00"
        --work-ram-patch
        "3354:3300:20"
        --work-ram-patch
        "3354:3380:02"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Tower bonus stage-selection recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 10"
    "Native replacement 03:60F9@all entries: 1 of 1"
    "Native replacement 03:6136@all entries: 1 of 1"
    "Observe hook 1 entries: 2405"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 1"
    "Observe hook 5 entries: 256"
    "Routine call trace complete: 2664 calls, 151300 writes"
    "Work-RAM write trace complete: 14530 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Tower bonus stage-selection output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census call write state_write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${state_write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL "${expected_${artifact}_sha256}")
        message(
            FATAL_ERROR
            "Tower bonus stage-selection ${artifact} artifact changed.\n"
            "Expected: ${expected_${artifact}_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
    message(STATUS "Tower bonus stage-selection ${artifact} verified: ${actual_sha256}")
endforeach()

message(
    STATUS
    "Tower bonus stage selection verified: geometry-delivered sign-bit collision and 6136 prefix."
)
