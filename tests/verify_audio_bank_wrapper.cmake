# SPDX-License-Identifier: GPL-3.0-or-later

# DC_NATIVE_ADMISSION 00:ED58 8 8
# DC_NATIVE_ADMISSION 00:ED75 8 8
foreach(required IN ITEMS TEST_APP ROM)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
execute_process(COMMAND "${TEST_APP}" "${ROM}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Audio bank-wrapper replay failed.\n${output}${error}")
endif()
foreach(line IN ITEMS
    "Verified 65536 stack/bank combinations."
    "Assembly evidence SHA-256: 27100f9e36da2843f115d507d72be90aacfe7b6a2c7e3411cad06b56148fd512"
    "composed=0 Native replacement 00:ED58@all entries: 8 of 8"
    "composed=0 Native replacement 00:ED75@all entries: 8 of 8"
    "composed=1 Native replacement 00:ED58@all entries: 8 of 8"
    "composed=1 Native replacement 00:ED75@all entries: 8 of 8")
    string(FIND "${output}" "${line}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Audio bank-wrapper evidence changed: ${line}\n${output}")
    endif()
endforeach()
message(STATUS "${output}")
