# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported music-transpose-dispatch configuration: ${CONFIG}")
endif()

set(expected_state_sha256 "77568124b24b80006d3077da8fcf0df21706c5627278e4c7bc92f645e56773cf")
set(expected_call_sha256 "385ab3214a970cec5503da351f98fd4dde531da4f4acacaee3ecaa00af159977")
set(expected_call_write_sha256 "429e95849a15eec87ce5bf3db4b68c9db48495f007d32ca51301e0cfe00f4d57")
set(expected_write_sha256 "1081bbf87bdcdcc46fecfc6c0105f9ea856df011fece8536e543232c24418d6b")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${OUTPUT}.writes.csv")
execute_process(
    COMMAND "${APP}"
        --record-gameplay-higharc "${OUTPUT}"
        --observe-hook "45:4368"
        --replace-hook "45:4368@all"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2E90-2EA8"
        --work-ram-write-trace "${write_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Music transpose-dispatch recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Gameplay higharc recording complete: 3600 frames"
    "Native replacement 45:4368@all entries: 149 of 152"
    "Observe hook 1 entries: 152"
    "Routine call trace complete: 152 calls, 152 writes"
    "Work-RAM write trace complete: 3972 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Music transpose-dispatch output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Music transpose-dispatch ${artifact} artifact changed.")
    endif()
endforeach()

message(STATUS "Music transpose dispatch preserves all high-arc state, calls and writes natively.")
