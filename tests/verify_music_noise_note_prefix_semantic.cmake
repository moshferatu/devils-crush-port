# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported music-noise-note-prefix configuration: ${CONFIG}")
endif()

# The duration decoder's noise-note prefix has a pre-retirement interrupt
# sample on its indexed load. It runs atomically when its 27-cycle window fits
# and deliberately leaves horizon crossings in the core; all observable high
# arc state, call, and write artifacts must remain byte-for-byte identical.
set(expected_state_sha256 "77568124b24b80006d3077da8fcf0df21706c5627278e4c7bc92f645e56773cf")
set(expected_call_sha256 "da23a432bc4a52fd338a288bb98f32e6fcbbec2b3cf5eaa7c512322029b61a03")
set(expected_call_write_sha256 "cfd72afacad038e505c618ec7ad00410f269559af7d56cdb8f64070fdf01e21d")
set(expected_write_sha256 "c0d9d52c29fbe67c54ea63502cead469dcd909727eecd92dd1b307c4d90d7470")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${OUTPUT}.writes.csv")
execute_process(
    COMMAND "${APP}"
        --record-gameplay-higharc "${OUTPUT}"
        --observe-hook "45:46E2"
        --replace-hook "45:46E2@all"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2E40-2E50"
        --work-ram-write-trace "${write_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Music noise-note-prefix recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Gameplay higharc recording complete: 3600 frames"
    "Native replacement 45:46E2@all entries: 1801 of 2067"
    "Observe hook 1 entries: 2067"
    "Routine call trace complete: 2067 calls, 19975 writes"
    "Work-RAM write trace complete: 519 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Music noise-note-prefix output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
            "Music noise-note-prefix ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Music noise-note prefix preserves all high-arc state, calls and writes natively.")
