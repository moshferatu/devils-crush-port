# SPDX-License-Identifier: GPL-3.0-or-later
# Natural census: palette chunk boundary, frame delay, physics returns,
# delayed-spawn countdown (waiting and expiry), and music voice clearing.
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(keys "00:EB1A" "00:E32C" "01:D62D" "01:D68D" "00:E32F" "00:F55B" "03:7D6F" "45:4B36")
set(offsets 2842 812 13869 13965 815 5467 32111 305974)
set(opcodes 4cb0ea 2021e3 60 60 c62c 60 de003a 9e452e)
foreach(index RANGE 0 7)
    list(GET offsets ${index} offset)
    list(GET opcodes ${index} expected)
    string(LENGTH "${expected}" length)
    math(EXPR length "${length} / 2")
    file(READ "${ROM}" bytes OFFSET ${offset} LIMIT ${length} HEX)
    if(NOT bytes STREQUAL expected)
        message(FATAL_ERROR "Ranked control ROM bytes changed at index ${index}")
    endif()
endforeach()
# Hashes below were captured independently with every replacement disabled.
set(expected_csv "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_calls.csv "e4bd51ee990a1a3f552b703a60930315bcf47fb3348e6339b2ec364f3cb3a5cd")
set(expected_calls.csv.writes.csv "af96848828ca84e95b978711b68be0eb26958e8ad9809b1ddf3b3546ea4f7621")
set(expected_writes.csv "2061f481a31f4fde9b12877df8aecc25b23b52205be0b208e9476417f938f620")
set(expected_bus.csv "5a8a45526ec153e2bfd8921b081719f216ad93cd5d10f78da86074405a9922cd")
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        foreach(key IN LISTS keys)
            list(APPEND hooks --replace-hook "${key}@all")
        endforeach()
    endif()
    set(base "${OUTPUT}.${mode}")
    execute_process(COMMAND "${APP}" --record-state-census "${base}.csv" ${hooks}
        --observe-hook "01:C693" --observe-hook "01:C696"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --routine-call-trace "${base}.calls.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${base}.writes.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ranked control ${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        foreach(expected IN ITEMS
            "Native replacement 00:EB1A@all entries: 3840 of 3840"
            "Native replacement 00:E32C@all entries: 44 of 44"
            "Native replacement 01:D62D@all entries: 52 of 62"
            "Native replacement 01:D68D@all entries: 32 of 36"
            "Native replacement 00:E32F@all entries: 44 of 44"
            "Native replacement 00:F55B@all entries: 35 of 40"
            "Native replacement 03:7D6F@all entries: 193 of 193"
            "Native replacement 45:4B36@all entries: 50 of 50"
            "Routine call trace complete: 2596 calls, 876882 writes"
            "Work-RAM write trace complete: 12647397 selected writes"
            "Bus write trace complete: 3149357 selected writes")
            string(FIND "${output}" "${expected}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "Missing ${expected}: ${output}${error}")
            endif()
        endforeach()
    endif()
    foreach(ext IN ITEMS csv calls.csv calls.csv.writes.csv writes.csv bus.csv)
        file(SHA256 "${base}.${ext}" actual)
        if(NOT actual STREQUAL "${expected_${ext}}")
            message(FATAL_ERROR "Ranked control ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "All eight ranked control sites preserve assembly frame, scheduler, RAM and device traces.")
