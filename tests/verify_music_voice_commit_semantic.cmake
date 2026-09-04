# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported music-voice-commit configuration: ${CONFIG}")
endif()

# This voice-commit leaf updates the channel record after the tick's volume
# branch. Its transcript retains the existing interrupt guard: calls that do
# not fit its live deadline are deliberately returned to assembly.
set(expected_state_sha256 "77568124b24b80006d3077da8fcf0df21706c5627278e4c7bc92f645e56773cf")
set(expected_call_sha256 "5824caa7d58e38b462fa3b7408eeff2b6f01dce92d7b18fe646796cad3aeeb8f")
set(expected_call_write_sha256 "c57fd2bffcc3f9942163e1a09b7d5772ec8e946540d5747c6ee07262b417f57c")
set(expected_write_sha256 "b65837e827a3b99baa11cebf5cc440ba8604c043d7d0e7e34a4e7b5db39111a8")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${OUTPUT}.writes.csv")
execute_process(
    COMMAND "${APP}"
        --record-gameplay-higharc "${OUTPUT}"
        --observe-hook "45:4A16"
        --replace-hook "45:4A16@all"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2E00-2FFF"
        --work-ram-write-trace "${write_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Music voice-commit recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Gameplay higharc recording complete: 3600 frames"
    "Native replacement 45:4A16@all entries: 20854 of 20865"
    "Observe hook 1 entries: 20865"
    "Routine call trace complete: 20865 calls, 130832 writes"
    "Work-RAM write trace complete: 326369 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Music voice-commit output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
            "Music voice-commit ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Music voice commit preserves high-arc calls, writes and frame state within its timer horizon.")
