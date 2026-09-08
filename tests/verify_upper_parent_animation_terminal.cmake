# SPDX-License-Identifier: GPL-3.0-or-later
# The natural startup switches scenes before the final ascending frame. Force
# that last counter transition in the real scheduled actor, not in the ROM.
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        foreach(pc IN ITEMS 705C 705F 7084 7087 7094 709E 70A7 70C1)
            list(APPEND hooks --replace-hook "09:${pc}@all")
        endforeach()
    endif()
    set(base "${OUTPUT}.${mode}")
    execute_process(COMMAND "${APP}" --record-gameplay "${base}.csv" ${hooks}
        --work-ram-patch "160:3A05:01" --work-ram-patch "160:3985:05"
        --observe-hook "09:70A7" --observe-hook "09:70C1"
        --routine-call-trace "${base}.calls.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Terminal ${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        if(NOT output MATCHES "Native replacement 09:70A7@all entries: ([0-9]+) of ([0-9]+)")
            message(FATAL_ERROR "Missing terminal admission: ${output}")
        endif()
        if(NOT CMAKE_MATCH_1 EQUAL 4 OR NOT CMAKE_MATCH_2 EQUAL 4)
            message(FATAL_ERROR "Terminal phase was not fully native: ${output}")
        endif()
    endif()
    string(FIND "${output}" "Observe hook 2 entries: 2" terminal)
    if(terminal EQUAL -1)
        message(FATAL_ERROR "Terminal audio branch not reached: ${output}")
    endif()
endforeach()
foreach(suffix IN ITEMS csv calls.csv calls.csv.writes.csv bus.csv)
    file(SHA256 "${OUTPUT}.assembly.${suffix}" original)
    file(SHA256 "${OUTPUT}.native.${suffix}" replacement)
    if(NOT original STREQUAL replacement)
        message(FATAL_ERROR "Terminal ${suffix}: ${original} != ${replacement}")
    endif()
    message(STATUS "terminal ${suffix}: ${original}")
endforeach()
