# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(keys "03:7DCC" "03:7DD5" "03:7DD8")
set(offsets 32204 32213 32216)
set(opcodes bd003b 9d003b 1d803b)
foreach(index RANGE 0 2)
    list(GET offsets ${index} offset)
    list(GET opcodes ${index} expected)
    string(LENGTH "${expected}" length)
    math(EXPR length "${length} / 2")
    file(READ "${ROM}" bytes OFFSET ${offset} LIMIT ${length} HEX)
    if(NOT bytes STREQUAL expected)
        message(FATAL_ERROR "Ranked soldier countdown opcode changed at index ${index}")
    endif()
endforeach()
set(expected_csv "64b35e910b7a0e5adeed46e3131f9d069b54c8ef9fdfbad4de08e87f90507750")
set(expected_calls.csv "927e7c771989ccd2da5ce8c10431cec976d015c40e940f1fce51a8f689ac4d90")
set(expected_calls.csv.writes.csv "32f80921ce11fae23374c8fd7a2177664dd2cd2ed804331cda9ae4ba4a93a6f0")
set(expected_writes.csv "c15ed438156019d6e50c74880267a8896fb9e72303b6c719257df6d7d48998f7")
set(expected_bus.csv "8f8fe72347663459f436b39836dd20ca5a6cfb90947db5422541868f37b071e8")
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        foreach(key IN LISTS keys)
            list(APPEND hooks --replace-hook "${key}@all")
        endforeach()
    endif()
    set(base "${OUTPUT}.${mode}")
    # Activate the Eyeball and two fixed scheduler slots at real countdown
    # entry points. The 256-tick counter includes its zero-low-byte borrow;
    # the bank-03 actor then continues through its normal animation cycle.
    execute_process(COMMAND "${APP}" --record-state-census "${base}.csv" ${hooks}
        --work-ram-patch "1200:2D20:01"
        --work-ram-patch "1200:3344:2C" --work-ram-patch "1200:33C4:01"
        --work-ram-patch "1200:3F44:18" --work-ram-patch "1200:3FC4:43"
        --work-ram-patch "1200:3B44:00" --work-ram-patch "1200:3BC4:01"
        --work-ram-patch "1200:3346:2C" --work-ram-patch "1200:33C6:01"
        --work-ram-patch "1200:3F46:C8" --work-ram-patch "1200:3FC6:7D"
        --work-ram-patch "1200:3B46:C0" --work-ram-patch "1200:3BC6:00"
        --observe-hook "01:C693" --observe-hook "01:C696"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --routine-call-trace "${base}.calls.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${base}.writes.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(WRITE "${base}.log" "${output}${error}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ranked soldier countdown ${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        foreach(expected IN ITEMS
    "Native replacement 03:7DCC@all entries: 384 of 384"
    "Native replacement 03:7DD5@all entries: 384 of 384"
    "Native replacement 03:7DD8@all entries: 384 of 384"
    "Routine call trace complete: 1817 calls, 946403 writes"
    "Work-RAM write trace complete: 11941811 selected writes"
    "Bus write trace complete: 3410099 selected writes")
            string(FIND "${output}" "${expected}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "Missing ${expected}: ${output}${error}")
            endif()
        endforeach()
    endif()
    foreach(ext IN ITEMS csv calls.csv calls.csv.writes.csv writes.csv bus.csv)
        file(SHA256 "${base}.${ext}" actual)
        if(NOT actual STREQUAL "${expected_${ext}}")
            message(FATAL_ERROR "Ranked soldier countdown ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Soldier countdown sites preserve assembly frame, scheduler, RAM and device traces.")
