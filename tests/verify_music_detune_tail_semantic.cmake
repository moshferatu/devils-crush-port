# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported music-detune-tail configuration: ${CONFIG}")
endif()

# The bank-45 detune tail is a leaf reached from the command interpreter. Its
# existing sequencer model already transcribes its BEQ/ADC/mask/store sequence;
# this route proves it can also own the leaf's live caller frame and RTS.
set(expected_state_sha256 "77568124b24b80006d3077da8fcf0df21706c5627278e4c7bc92f645e56773cf")
set(expected_call_sha256 "830d06497783ef9613f70278e360e35d2638fc7513f181775316c2c76f79710c")
set(expected_call_write_sha256 "b0547862685bf3f5bdbb8e835f387c8892a2cda109ef0d1aa8a85a3bc86fb74c")
set(expected_write_sha256 "2df3fda5eeb92efc0198df78a4a9925fb4948531028f27554122a7333eefdbce")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${OUTPUT}.writes.csv")
execute_process(
    COMMAND "${APP}"
        --record-gameplay-higharc "${OUTPUT}"
        --observe-hook "45:4434"
        --replace-hook "45:4434@all"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2F68-2F70"
        --work-ram-write-trace "${write_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Music detune-tail recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Gameplay higharc recording complete: 3600 frames"
    "Native replacement 45:4434@all entries: 122 of 122"
    "Observe hook 1 entries: 122"
    "Routine call trace complete: 122 calls, 122 writes"
    "Work-RAM write trace complete: 704 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Music detune-tail output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
            "Music detune-tail ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Music detune tail preserves all high-arc leaf calls, writes and frame state natively.")
