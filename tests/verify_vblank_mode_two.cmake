# SPDX-License-Identifier: GPL-3.0-or-later
# DC_NATIVE_ADMISSION 00:E09E 203 203
# The route-dependent assertion below proves the alley count and 187 spiral entries.
# Release retains the complete RAM stream. Debug pins every RAM location this
# handler can write, as the existing VBlank regression does; full RAM is still
# checked in each frame fingerprint. This avoids serializing unrelated actor
# and bulk-upload writes through the Debug stream formatter.
set(ram_ranges --work-ram-write-range 2000-3FFF)
if(CONFIG STREQUAL "Debug")
    set(ram_ranges --work-ram-write-range 2000-2002
        --work-ram-write-range 200E-201C --work-ram-write-range 20D9-20DA
        --work-ram-write-range 2100-21FF --work-ram-write-range 222A-222B)
endif()
foreach(route IN ITEMS alley spiral)
    if(route STREQUAL "alley")
        set(expected 203)
    else()
        set(expected 187)
    endif()
    foreach(mode IN ITEMS assembly native)
        set(hooks)
        if(mode STREQUAL "native")
            set(hooks --replace-hook "00:E09E@all" --replace-hook "00:E05F@all")
        endif()
        set(base "${OUTPUT}.${route}.${mode}")
        execute_process(COMMAND "${APP}" "--record-gameplay-${route}" "${base}.csv"
            ${hooks} ${ram_ranges}
            --work-ram-write-trace "${base}.writes.csv"
            --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
            --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "${route}/${mode}: ${output}${error}")
        endif()
        if(mode STREQUAL "native")
            string(FIND "${output}" "Native replacement 00:E09E@all entries: ${expected} of ${expected}" admitted)
            if(admitted LESS 0)
                message(FATAL_ERROR "Mode-2 admission changed: ${output}")
            endif()
        endif()
    endforeach()
    foreach(suffix IN ITEMS csv writes.csv bus.csv)
        file(SHA256 "${OUTPUT}.${route}.assembly.${suffix}" original)
        file(SHA256 "${OUTPUT}.${route}.native.${suffix}" replacement)
        if(NOT original STREQUAL replacement)
            message(FATAL_ERROR "${route} VBlank mode 2 ${suffix}: ${original} != ${replacement}")
        endif()
    endforeach()
endforeach()
