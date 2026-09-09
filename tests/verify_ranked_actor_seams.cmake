# SPDX-License-Identifier: GPL-3.0-or-later
# Controlled actor activation exercises every address in the September 9 ranking.
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
file(READ "${ROM}" bytes OFFSET 17833 LIMIT 1 HEX)
if(NOT bytes STREQUAL "60")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 02:45A9")
endif()
file(READ "${ROM}" bytes OFFSET 17777 LIMIT 1 HEX)
if(NOT bytes STREQUAL "60")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 02:4571")
endif()
file(READ "${ROM}" bytes OFFSET 17828 LIMIT 3 HEX)
if(NOT bytes STREQUAL "adfa2c")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 02:45A4")
endif()
file(READ "${ROM}" bytes OFFSET 23028 LIMIT 1 HEX)
if(NOT bytes STREQUAL "60")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 02:59F4")
endif()
file(READ "${ROM}" bytes OFFSET 17540 LIMIT 1 HEX)
if(NOT bytes STREQUAL "60")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 02:4484")
endif()
file(READ "${ROM}" bytes OFFSET 22187 LIMIT 1 HEX)
if(NOT bytes STREQUAL "60")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 02:56AB")
endif()
file(READ "${ROM}" bytes OFFSET 1546 LIMIT 1 HEX)
if(NOT bytes STREQUAL "60")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 00:E60A")
endif()
file(READ "${ROM}" bytes OFFSET 25010 LIMIT 1 HEX)
if(NOT bytes STREQUAL "60")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 03:61B2")
endif()
file(READ "${ROM}" bytes OFFSET 17767 LIMIT 3 HEX)
if(NOT bytes STREQUAL "bc003e")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 02:4567")
endif()
file(READ "${ROM}" bytes OFFSET 17770 LIMIT 3 HEX)
if(NOT bytes STREQUAL "b9f82c")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 02:456A")
endif()
file(READ "${ROM}" bytes OFFSET 22132 LIMIT 3 HEX)
if(NOT bytes STREQUAL "200a57")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 02:5674")
endif()
file(READ "${ROM}" bytes OFFSET 22305 LIMIT 1 HEX)
if(NOT bytes STREQUAL "60")
    message(FATAL_ERROR "Ranked actor ROM bytes changed at 02:5721")
endif()
set(expected_assembly_csv "ab62b7570b716e3b0c1320bc465258f2a35a53ad42c1a081a709f3af4730eb1d")
set(expected_assembly_calls.csv "82d488125711027205e9b3b696d24b833e7ba1d0f2a5ce9b781f6af5f556c43e")
set(expected_assembly_calls.csv.writes.csv "ffb27d4962e42886f05ccb5a15cd40c33d26e210586f1ea722ab6794074e628d")
set(expected_assembly_writes.csv "05cb8e876be1ad6acd9b55b655aedba68f27c05e2684411f862e0dd5deb47c32")
set(expected_assembly_bus.csv "ed9bd9ec08b34b7daf44e95ee98026422039f652a9624506f714e1a93616028e")
set(expected_native_csv "ab62b7570b716e3b0c1320bc465258f2a35a53ad42c1a081a709f3af4730eb1d")
set(expected_native_calls.csv "b7eb0f8731792e2498b0236a5b3fa5528796d23b78d0aea6d0a7033966f39f6e")
set(expected_native_calls.csv.writes.csv "a4d4695a916331c057a652c6335f0f185d7d81a93e81b92c278b0847992fc7c1")
set(expected_native_writes.csv "05cb8e876be1ad6acd9b55b655aedba68f27c05e2684411f862e0dd5deb47c32")
set(expected_native_bus.csv "ed9bd9ec08b34b7daf44e95ee98026422039f652a9624506f714e1a93616028e")
# The existing full 5674 walk hides its child RTS from the C5C9 observer.
# Native call hashes below match the saved pre-change binary with only that
# existing walk enabled; machine, RAM-write and bus-write hashes match assembly.
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        set(hooks
            --replace-hook "02:45A9@all"
            --replace-hook "02:4571@all"
            --replace-hook "02:45A4@all"
            --replace-hook "02:59F4@all"
            --replace-hook "02:4484@all"
            --replace-hook "02:56AB@all"
            --replace-hook "00:E60A@all"
            --replace-hook "03:61B2@all"
            --replace-hook "02:4567@all"
            --replace-hook "02:456A@all"
            --replace-hook "02:5674@all"
            --replace-hook "02:5721@all"
        )
    endif()
    set(base "${OUTPUT}.${mode}")
    execute_process(COMMAND "${APP}" --record-state-census "${base}.csv" ${hooks}
        --work-ram-patch "1200:3340:2C" --work-ram-patch "1200:33C0:01"
        --work-ram-patch "1200:3344:2C" --work-ram-patch "1200:33C4:01"
        --work-ram-patch "1200:3346:2C" --work-ram-patch "1200:33C6:01"
        --work-ram-patch "1200:3347:2C" --work-ram-patch "1200:33C7:01"
        --work-ram-patch "1200:334B:2C" --work-ram-patch "1200:33CB:01"
        --work-ram-patch "1200:334D:2C" --work-ram-patch "1200:33CD:01"
        --work-ram-patch "1200:3F46:A4"
        --work-ram-patch "1200:3FC6:45"
        --work-ram-patch "1200:3F47:67"
        --work-ram-patch "1200:3FC7:45"
        --work-ram-patch "1200:2CFA:01"
        --observe-hook "01:C693" --observe-hook "01:C696"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --routine-call-trace "${base}.calls.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${base}.writes.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ranked actors ${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        foreach(expected IN ITEMS
            "Native replacement 02:45A9@all entries: 1 of 1"
            "Native replacement 02:4571@all entries: 370 of 432"
            "Native replacement 02:45A4@all entries: 2 of 2"
            "Native replacement 02:59F4@all entries: 271 of 310"
            "Native replacement 02:4484@all entries: 18 of 21"
            "Native replacement 02:56AB@all entries: 383 of 431"
            "Native replacement 00:E60A@all entries: 49 of 56"
            "Native replacement 03:61B2@all entries: 372 of 431"
            "Native replacement 02:4567@all entries: 432 of 432"
            "Native replacement 02:456A@all entries: 432 of 432"
            "Native replacement 02:5674@all entries: 384 of 431"
            "Native replacement 02:5721@all entries: 174 of 224"
        )
            string(FIND "${output}" "${expected}" position)
            if(position EQUAL -1)
                message(FATAL_ERROR "Missing ${expected}: ${output}")
            endif()
            message(STATUS "${expected}")
        endforeach()
    endif()
    foreach(suffix IN ITEMS csv calls.csv calls.csv.writes.csv writes.csv bus.csv)
        file(SHA256 "${base}.${suffix}" actual)
        if(NOT actual STREQUAL "${expected_${mode}_${suffix}}")
            message(FATAL_ERROR "Ranked actors ${mode} ${suffix}: ${actual} changed")
        endif()
    endforeach()
endforeach()
foreach(suffix IN ITEMS csv writes.csv bus.csv)
    file(SHA256 "${OUTPUT}.assembly.${suffix}" original)
    file(SHA256 "${OUTPUT}.native.${suffix}" replacement)
    if(NOT original STREQUAL replacement)
        message(FATAL_ERROR "Ranked actor ${suffix} differs from assembly")
    endif()
    message(STATUS "${suffix}: ${original}")
endforeach()
