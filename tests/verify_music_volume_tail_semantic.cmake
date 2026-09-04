# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported music-volume-tail configuration: ${CONFIG}")
endif()

# This is the negative-delta volume clamp tail reached by the bank-45 command
# interpreter. The sequencer already owns its instructions; the regression
# proves that entering at the leaf preserves its caller frame and RTS.
set(expected_state_sha256 "77568124b24b80006d3077da8fcf0df21706c5627278e4c7bc92f645e56773cf")
set(expected_call_sha256 "814d1f48f781ad26485cb33f63d7bd026f016c58a228546d2cb68a0bdef34d4f")
set(expected_call_write_sha256 "5269d2c7c37f312ad98a3a077d692f2190cd37f4f031b1be8f44c59e1cdb4b0f")
set(expected_write_sha256 "c242e086c6152daa87fe9cfd50bc115eb74445c75aaef909f570d96ac4a320b3")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${OUTPUT}.writes.csv")
execute_process(
    COMMAND "${APP}"
        --record-gameplay-higharc "${OUTPUT}"
        --observe-hook "45:4389"
        --replace-hook "45:4389@all"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2E50-2E58"
        --work-ram-write-trace "${write_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Music volume-tail recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Gameplay higharc recording complete: 3600 frames"
    "Native replacement 45:4389@all entries: 34 of 34"
    "Observe hook 1 entries: 34"
    "Routine call trace complete: 34 calls, 34 writes"
    "Work-RAM write trace complete: 405 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Music volume-tail output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS state call call_write write)
    if(artifact STREQUAL "state")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "call_write")
        set(artifact_path "${call_output}.writes.csv")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR
            "Music volume-tail ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Music volume tail preserves all high-arc leaf calls, writes and frame state natively.")
