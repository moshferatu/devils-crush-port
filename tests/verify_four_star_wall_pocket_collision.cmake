# SPDX-License-Identifier: GPL-3.0-or-later

# This focused route is separate from the steady-controller regression: it
# asserts the unique collision gate before the original access, scoring, and
# bonus-transition continuation resumes in assembly.
foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(expected_census_sha256 "d7a897a7c019c8b618176c9b92371d08f82f602b3ba17cd5fede2e8dcf0c000c")
elseif(CONFIG STREQUAL "Release")
    set(expected_census_sha256 "53432539e70810d69ef8b3cce45cd81300749ea021f27f4fd71e8587ccfbe28e")
else()
    message(FATAL_ERROR "Unsupported Four-Star Wall-Pocket collision configuration: ${CONFIG}")
endif()
set(expected_trace_sha256 "a4bcc05a6cd9e7ae65ee875b2bd8bbd2c74dc15451ecf57b274fc1971aa6ad00")
set(expected_call_sha256 "06138a08be3eed6811faa144a1a04f48d2b2ba1ad9c3c6eab2ba7bf3e7382c5a")
set(expected_write_sha256 "f822e87f6a423a9de0d00f10ae6bea73bde54c314c734d13c740f86886ccb9fc")
set(expected_state_sha256 "a019a78ef19fc7d72e11b329aaf90bb390e50cf6728a56aa897c1a974664069a")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "873:383E:80"
        --observe-hook "02:4A93"
        --replace-hook "02:4A93@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "2CFA-2CFA"
        --work-ram-write-range "2D00-2D01"
        --work-ram-write-range "2D18-2D18"
        --work-ram-write-range "2D33-2D33"
        --work-ram-write-range "2D40-2D40"
        --work-ram-write-range "303E-303E"
        --work-ram-write-range "373E-373E"
        --work-ram-write-range "3780-3780"
        --work-ram-write-range "383E-383E"
        --work-ram-write-range "393E-393E"
        --work-ram-write-range "3A3E-3A3E"
        --work-ram-write-range "3C3E-3C3E"
        --work-ram-write-range "3D3E-3D3E"
        --work-ram-write-range "3E3E-3E3E"
        --work-ram-write-range "3F3E-3F3E"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Four-Star Wall-Pocket collision recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 1"
    "Native replacement 02:4A93@all entries: 1168 of 1168"
    "Observe hook 1 entries: 1168"
    "Routine trace complete: 1168 entries"
    "Routine call trace complete: 1168 calls, 2307 writes"
    "Work-RAM write trace complete: 4530 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Four-Star Wall-Pocket collision output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write state)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${state_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set("actual_${artifact}_sha256" "${actual_sha256}")
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Four-Star Wall-Pocket collision ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Four-Star Wall-Pocket collision artifacts verified: ${actual_census_sha256}, ${actual_trace_sha256}, ${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}")
