# SPDX-License-Identifier: GPL-3.0-or-later
# DC_NATIVE_ADMISSION 45:4B7B 5 5
# DC_NATIVE_ADMISSION 45:4B82 5 5
execute_process(COMMAND "${TEST_APP}" "${ROM}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Audio stack wrapper: ${output}${error}")
endif()
foreach(line IN ITEMS
    "Verified 16384 stack/status combinations."
    "Assembly evidence SHA-256: 553df59c829b86bc4a3f292c28f58094fb03255f0b983ddcfdfe7320eacc380f"
    "Native replacement 45:4B7B@all entries: 5 of 5"
    "Native replacement 45:4B82@all entries: 5 of 5")
    string(FIND "${output}" "${line}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing evidence ${line}: ${output}")
    endif()
endforeach()
message(STATUS "${output}")

# The gameplay route reaches the PLP-sensitive frame missed by the initial
# menu-only replay. Compare the whole enclosing call and all hardware writes.
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        set(hooks --replace-hook "45:4B7B@all" --replace-hook "45:4B82@all")
    endif()
    set(base "${OUTPUT}.${mode}")
    execute_process(COMMAND "${APP}" --record-gameplay "${base}.csv"
        ${hooks} --observe-hook "45:4B7B" --routine-call-trace "${base}.calls.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        foreach(pc IN ITEMS 4B7B 4B82)
            string(REGEX MATCH "Native replacement 45:${pc}@all entries: ([0-9]+) of ([0-9]+)" admission "${output}")
            if(NOT admission OR CMAKE_MATCH_1 EQUAL 0 OR NOT CMAKE_MATCH_1 EQUAL CMAKE_MATCH_2)
                message(FATAL_ERROR "Incomplete gameplay admission: ${output}")
            endif()
        endforeach()
    endif()
endforeach()
foreach(suffix IN ITEMS csv calls.csv calls.csv.writes.csv bus.csv)
    file(SHA256 "${OUTPUT}.assembly.${suffix}" original)
    file(SHA256 "${OUTPUT}.native.${suffix}" replacement)
    if(NOT original STREQUAL replacement)
        message(FATAL_ERROR "Audio wrapper ${suffix}: ${original} != ${replacement}")
    endif()
endforeach()
