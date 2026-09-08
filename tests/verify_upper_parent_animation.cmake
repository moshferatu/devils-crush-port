# SPDX-License-Identifier: GPL-3.0-or-later
# 70C1 is an existing score-panel scheduler owner, retained unchanged. Include
# it to verify the new controller's final audio-return handoff into that owner.
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
function(check_natural_counts output)
    foreach(expected IN ITEMS
        "Native replacement 09:705C@all entries: 36 of 36"
        "Native replacement 09:705F@all entries: 36 of 36"
        "Native replacement 09:7084@all entries: 33 of 33"
        "Native replacement 09:7087@all entries: 33 of 33"
        "Native replacement 09:7094@all entries: 1 of 1"
        "Native replacement 09:709E@all entries: 1 of 1"
        "Native replacement 09:70A7@all entries: 23 of 23"
        "Native replacement 09:70C1@all entries: 1 of 1")
        string(FIND "${output}" "${expected}" offset)
        if(offset EQUAL -1)
            message(FATAL_ERROR "Natural admission count changed: ${output}")
        endif()
    endforeach()
endfunction()
foreach(route IN ITEMS gameplay alternate)
    set(flag --record-gameplay)
    if(route STREQUAL "alternate")
        set(flag --record-gameplay-alternate)
    endif()
    foreach(mode IN ITEMS assembly native)
        set(hooks)
        if(mode STREQUAL "native")
            foreach(pc IN ITEMS 705C 705F 7084 7087 7094 709E 70A7 70C1)
                list(APPEND hooks --replace-hook "09:${pc}@all")
            endforeach()
        endif()
        set(base "${OUTPUT}.${route}.${mode}")
        execute_process(COMMAND "${APP}" ${flag} "${base}.csv" ${hooks}
            --observe-hook "09:705C" --observe-hook "09:7084" --observe-hook "09:70A7"
            --routine-call-trace "${base}.calls.csv"
            --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
            --rom "${ROM}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Parent animation ${route}/${mode}: ${output}${error}")
        endif()
        if(mode STREQUAL "native")
            check_natural_counts("${output}")
            foreach(pc IN ITEMS 705C 705F 7084 7087 7094 709E 70A7 70C1)
                if(NOT output MATCHES "Native replacement 09:${pc}@all entries: ([0-9]+) of ([0-9]+)")
                    message(FATAL_ERROR "Missing entry ${pc}: ${output}")
                endif()
                if(NOT CMAKE_MATCH_1 EQUAL CMAKE_MATCH_2 OR CMAKE_MATCH_1 EQUAL 0)
                    message(FATAL_ERROR "Unproven entry ${pc}: ${output}")
                endif()
                message(STATUS "${route} ${pc}: ${CMAKE_MATCH_1}/${CMAKE_MATCH_2}")
            endforeach()
        endif()
    endforeach()
    foreach(suffix IN ITEMS csv calls.csv calls.csv.writes.csv bus.csv)
        file(SHA256 "${OUTPUT}.${route}.assembly.${suffix}" original)
        file(SHA256 "${OUTPUT}.${route}.native.${suffix}" replacement)
        if(NOT original STREQUAL replacement)
            message(FATAL_ERROR "Parent ${route} ${suffix}: ${original} != ${replacement}")
        endif()
        message(STATUS "${route} ${suffix}: ${original}")
    endforeach()
endforeach()
