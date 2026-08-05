# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

set(assembly_output "${OUTPUT}.assembly.csv")
set(native_output "${OUTPUT}.native.csv")
set(assembly_calls "${assembly_output}.calls.csv")
set(native_calls "${native_output}.calls.csv")

set(common_arguments
    --work-ram-patch "3341:3E95:08"
    --observe-hook "03:78C7"
    --observe-hook "03:7998"
    --observe-hook "03:79DE"
    --observe-hook "03:7A71"
)

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${assembly_output}"
        ${common_arguments}
        --routine-call-trace "${assembly_calls}"
        --rom "${ROM}"
    RESULT_VARIABLE assembly_result
    OUTPUT_VARIABLE assembly_stdout
    ERROR_VARIABLE assembly_stderr
)
if(NOT assembly_result EQUAL 0)
    message(FATAL_ERROR
        "Bumper terminal assembly recorder failed (${assembly_result}).\n"
        "${assembly_stdout}${assembly_stderr}")
endif()

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${native_output}"
        ${common_arguments}
        --replace-hook "03:79DE@all"
        --routine-call-trace "${native_calls}"
        --rom "${ROM}"
    RESULT_VARIABLE native_result
    OUTPUT_VARIABLE native_stdout
    ERROR_VARIABLE native_stderr
)
if(NOT native_result EQUAL 0)
    message(FATAL_ERROR
        "Bumper terminal native recorder failed (${native_result}).\n"
        "${native_stdout}${native_stderr}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 1"
    "Observe hook 1 entries: 3321"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 6"
    "Observe hook 4 entries: 1202"
    "Routine call trace complete: 4530 calls, 23068 writes"
)
    string(FIND "${assembly_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR
            "Bumper terminal assembly output omitted '${expected_line}'.\n"
            "${assembly_stdout}${assembly_stderr}")
    endif()
endforeach()

foreach(expected_line IN ITEMS
    "Native replacement 03:79DE@all entries: 6 of 6"
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 1"
    "Observe hook 1 entries: 3321"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 6"
    "Observe hook 4 entries: 1202"
    "Routine call trace complete: 4530 calls, 23068 writes"
)
    string(FIND "${native_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR
            "Bumper terminal native output omitted '${expected_line}'.\n"
            "${native_stdout}${native_stderr}")
    endif()
endforeach()

foreach(artifact IN ITEMS census calls writes)
    if(artifact STREQUAL "census")
        set(assembly_artifact "${assembly_output}")
        set(native_artifact "${native_output}")
    elseif(artifact STREQUAL "calls")
        set(assembly_artifact "${assembly_calls}")
        set(native_artifact "${native_calls}")
    else()
        set(assembly_artifact "${assembly_calls}.writes.csv")
        set(native_artifact "${native_calls}.writes.csv")
    endif()
    file(SHA256 "${assembly_artifact}" assembly_sha256)
    file(SHA256 "${native_artifact}" native_sha256)
    if(NOT assembly_sha256 STREQUAL native_sha256)
        message(FATAL_ERROR
            "Bumper terminal ${artifact} artifact diverged from assembly.\n"
            "Assembly: ${assembly_sha256}\nNative:   ${native_sha256}\n")
    endif()
    message(STATUS "Bumper terminal ${artifact} matches assembly: ${native_sha256}")
endforeach()

