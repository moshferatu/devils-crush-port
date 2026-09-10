# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(keys "03:7E7E" "03:66A3" "03:7643" "03:7E6A" "45:485D" "03:7E76" "03:7E73" "02:567F" "03:66AA" "03:66B2" "00:E438" "03:66AD")
set(offsets 32382 26275 30275 32362 305245 32374 32371 22143 26282 26290 1080 26285)
set(opcodes 60 de003a 2018c0 bd003b 78 1d803b 9d003b de003a 9d003a 9d8039 c2 bd8039)
foreach(index RANGE 0 11)
    list(GET offsets ${index} offset)
    list(GET opcodes ${index} expected)
    string(LENGTH "${expected}" length)
    math(EXPR length "${length} / 2")
    file(READ "${ROM}" bytes OFFSET ${offset} LIMIT ${length} HEX)
    if(NOT bytes STREQUAL expected)
        message(FATAL_ERROR "Ranked timer opcode changed at index ${index}")
    endif()
endforeach()
set(expected_csv "1b96bbd4ed757318f0825a338f2d54155f165361e389d29f155dde63f5233a2e")
set(expected_calls.csv "e2d03614156b764e743fd50bd6ffa573f1fc9008b029c1e36ac8739592fceb06")
set(expected_calls.csv.writes.csv "4609f780f8e248b4e8d55fc7ea4be9ae1f731872f5dc095041b4c4fb71956d46")
set(expected_writes.csv "790683ae44c0edee30fbc6c266a7a7c1425c1533121127be919ffd81ee186bfb")
set(expected_bus.csv "e1c4253c527b577f124f2c4d1fa58fdf6caf6c4e3e3f49fb93d8c49e0c1d5e47")
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        foreach(key IN LISTS keys)
            list(APPEND hooks --replace-hook "${key}@all")
        endforeach()
    endif()
    set(base "${OUTPUT}.${mode}")
    # Activate the Big-Star controller and two fixed scheduler slots. The
    # 256-tick actor counter exercises its low-byte borrow and real expiry.
    execute_process(COMMAND "${APP}" --record-state-census "${base}.csv" ${hooks}
        --work-ram-patch "1200:334B:2C" --work-ram-patch "1200:33CB:01"
        --work-ram-patch "1200:3344:2C" --work-ram-patch "1200:33C4:01"
        --work-ram-patch "1200:3F44:43" --work-ram-patch "1200:3FC4:76"
        --work-ram-patch "1200:3346:2C" --work-ram-patch "1200:33C6:01"
        --work-ram-patch "1200:3F46:67" --work-ram-patch "1200:3FC6:7E"
        --work-ram-patch "1200:3B46:00" --work-ram-patch "1200:3BC6:01"
        --observe-hook "01:C693" --observe-hook "01:C696"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --routine-call-trace "${base}.calls.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${base}.writes.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ranked timer ${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        foreach(expected IN ITEMS
            "Native replacement 03:7E7E@all entries: 386 of 446"
            "Native replacement 03:66A3@all entries: 1301 of 1301"
            "Native replacement 03:7643@all entries: 62 of 73"
            "Native replacement 03:7E6A@all entries: 448 of 448"
            "Native replacement 45:485D@all entries: 324 of 324"
            "Native replacement 03:7E76@all entries: 448 of 448"
            "Native replacement 03:7E73@all entries: 448 of 448"
            "Native replacement 02:567F@all entries: 787 of 787"
            "Native replacement 03:66AA@all entries: 43 of 43"
            "Native replacement 03:66B2@all entries: 43 of 43"
            "Native replacement 00:E438@all entries: 19040 of 19040"
            "Native replacement 03:66AD@all entries: 43 of 43")
            string(FIND "${output}" "${expected}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "Missing ${expected}: ${output}${error}")
            endif()
        endforeach()
    endif()
    foreach(ext IN ITEMS csv calls.csv calls.csv.writes.csv writes.csv bus.csv)
        file(SHA256 "${base}.${ext}" actual)
        if(NOT actual STREQUAL "${expected_${ext}}")
            message(FATAL_ERROR "Ranked timer ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "All twelve ranked sites preserve assembly frame, scheduler, RAM and device traces.")
