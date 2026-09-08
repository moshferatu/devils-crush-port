# SPDX-License-Identifier: GPL-3.0-or-later
# This RTS dispatches a pushed table vector; it must not complete the caller.
foreach(route IN ITEMS gameplay alternate)
    set(flag --record-gameplay)
    set(expected_native 367)
    set(expected_entries 404)
    if(route STREQUAL "alternate")
        set(flag --record-gameplay-alternate)
        set(expected_native 224)
        set(expected_entries 246)
    endif()
    foreach(mode IN ITEMS assembly native)
        set(hooks)
        if(mode STREQUAL "native")
            set(hooks --replace-hook "01:CBF9@all")
        endif()
        set(base "${OUTPUT}.${route}.${mode}")
        execute_process(COMMAND "${APP}" ${flag} "${base}.csv"
            ${hooks} --observe-hook "01:CBF1" --routine-call-trace "${base}.calls.csv"
            --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
            --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "${route}/${mode}: ${output}${error}")
        endif()
        if(mode STREQUAL "native")
            string(REGEX MATCH "Native replacement 01:CBF9@all entries: ([0-9]+) of ([0-9]+)" admission "${output}")
            # The CLI conservatively declines checkpoint-free descriptors at
            # an interrupt horizon, even though RTS itself stays in the core.
            if(NOT admission OR NOT CMAKE_MATCH_1 EQUAL expected_native
                OR NOT CMAKE_MATCH_2 EQUAL expected_entries)
                message(FATAL_ERROR "Incomplete vector dispatch admission: ${output}")
            endif()
            message(STATUS "${route}: ${admission}")
        endif()
    endforeach()
    foreach(suffix IN ITEMS csv calls.csv calls.csv.writes.csv bus.csv)
        file(SHA256 "${OUTPUT}.${route}.assembly.${suffix}" original)
        file(SHA256 "${OUTPUT}.${route}.native.${suffix}" replacement)
        if(NOT original STREQUAL replacement)
            message(FATAL_ERROR "Collision vector ${route}/${suffix}: ${original} != ${replacement}")
        endif()
    endforeach()
endforeach()
