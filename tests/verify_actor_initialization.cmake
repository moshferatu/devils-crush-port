# SPDX-License-Identifier: GPL-3.0-or-later
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        set(hooks --replace-hook "01:C655@all" --replace-hook "01:C699@all"
            --replace-hook "01:C69E@all" --replace-hook "01:C6A9@all")
    endif()
    set(base "${OUTPUT}.${mode}")
    execute_process(COMMAND "${APP}" --record-gameplay "${base}.csv" ${hooks}
        --observe-hook 01:C655 --observe-hook 01:C699 --observe-hook 01:C69E --observe-hook 01:C6A9
        --routine-call-trace "${base}.calls.csv" --bus-write-range 0000-1FFF
        --bus-write-trace "${base}.bus.csv" --rom "${ROM}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        foreach(pc IN ITEMS C655 C699 C69E C6A9)
            string(REGEX MATCH "Native replacement 01:${pc}@all entries: ([0-9]+) of ([0-9]+)" admission "${output}")
            if(NOT admission OR CMAKE_MATCH_1 EQUAL 0 OR NOT CMAKE_MATCH_1 EQUAL CMAKE_MATCH_2)
                message(FATAL_ERROR "Incomplete actor initialization admission: ${output}")
            endif()
            message(STATUS "${admission}")
        endforeach()
        foreach(expected IN ITEMS
            "Native replacement 01:C655@all entries: 1 of 1"
            "Native replacement 01:C699@all entries: 1 of 1"
            "Native replacement 01:C69E@all entries: 48 of 48"
            "Native replacement 01:C6A9@all entries: 1 of 1")
            string(FIND "${output}" "${expected}" found)
            if(found LESS 0)
                message(FATAL_ERROR "Admission baseline changed: ${expected}")
            endif()
        endforeach()
    endif()
endforeach()
foreach(suffix IN ITEMS csv calls.csv calls.csv.writes.csv bus.csv)
    file(SHA256 "${OUTPUT}.assembly.${suffix}" original)
    file(SHA256 "${OUTPUT}.native.${suffix}" replacement)
    if(NOT original STREQUAL replacement)
        message(FATAL_ERROR "Actor initialization ${suffix}: ${original} != ${replacement}")
    endif()
endforeach()
