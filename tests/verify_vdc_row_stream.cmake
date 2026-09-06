# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(expected_csv "1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353")
set(expected_trace.csv "88b53bcb2b86376fa2877dfab9e7b87f0cb98986548895849a11845545387acc")
set(expected_writes.csv "da606d14dd48b5b73e86096ba4033c6bf40c67e97ea771e5705760a02bbe3e1c")
set(expected_bus.csv "0aa7c15c4768684c130ba4c1c58f252021c68cb47e44f036bc35f824b1fab51f")
# Original joypad route, with no RAM/ROM patches. Compare all ordered RAM and
# VDC writes (including instruction and master-cycle timestamps), complete
# frame/audio/RAM/video census and every row head plus the enclosing RTS.
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        set(hooks --replace-hook "00:EF2A@all")
    endif()
    execute_process(COMMAND "${APP}" --record-gameplay "${OUTPUT}.${mode}.csv"
        ${hooks} --observe-hook "00:EF2A" --observe-hook "00:EF6E"
        --routine-trace "${OUTPUT}.${mode}.trace.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${OUTPUT}.${mode}.writes.csv"
        --bus-write-range 0000-0003 --bus-write-trace "${OUTPUT}.${mode}.bus.csv"
        --rom "${ROM}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Row-stream ${mode} failed: ${output}${error}")
    endif()
    foreach(line IN ITEMS "Gameplay recording complete: 1800 frames"
        "Routine trace complete: 5 entries"
        "Work-RAM write trace complete: 4270592 selected writes"
        "Bus write trace complete: 1019098 selected writes")
        string(FIND "${output}" "${line}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "${mode} omitted ${line}: ${output}")
        endif()
    endforeach()
    if(mode STREQUAL "native")
        string(FIND "${output}" "Native replacement 00:EF2A@all entries: 4 of 4" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Row-stream admission changed: ${output}")
        endif()
    endif()
endforeach()
foreach(suffix IN ITEMS csv trace.csv writes.csv bus.csv)
    file(SHA256 "${OUTPUT}.assembly.${suffix}" assembly_hash)
    file(SHA256 "${OUTPUT}.native.${suffix}" native_hash)
    if(NOT assembly_hash STREQUAL native_hash OR NOT assembly_hash STREQUAL expected_${suffix})
        message(FATAL_ERROR "Row-stream ${suffix} differs: ${assembly_hash} / ${native_hash}")
    endif()
    message(STATUS "Row-stream ${suffix}: ${assembly_hash}")
endforeach()
