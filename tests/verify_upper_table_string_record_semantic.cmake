# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Upper-table string-record regression is Release-only.")
endif()

# 08:5ABF is the upper-table text-record renderer.  Its FE continuation is a
# real tail jump back to 5ABF, so the native transcript deliberately stops at
# that boundary and the registry resumes the next record.  This route covers
# all 18 observed entries, including ROM-backed records and the VDC traffic
# emitted by each composed 5B01 glyph write.
set(expected_census_sha256 "1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353")
set(expected_work_ram_sha256 "afcd4153f949fdf9358e652b32634f48e79b0bf9918a5e65478341c6c956abc4")
set(expected_framebuffer_sha256 "dc3eefa91829aee84823e69a7b21feb7208227657d280b4ebcffba5449bc6c1b")
set(expected_ram_sha256 "36c6a5232e16d68437b9cfc1e1c20491b4ee2c532d1857c0edb381440bcc4b7b")
set(expected_bus_sha256 "0aa7c15c4768684c130ba4c1c58f252021c68cb47e44f036bc35f824b1fab51f")

set(assembly_census "${OUTPUT}.assembly.csv")
set(native_census "${OUTPUT}.native.csv")

function(record_route census suffix)
    execute_process(
        COMMAND
            "${APP}"
            --record-gameplay "${census}"
            --canonical-work-ram "${census}.work-ram.bin"
            --canonical-framebuffer "${census}.framebuffer.rgb"
            --work-ram-write-range "202B-2030"
            --work-ram-write-trace "${census}.ram.csv"
            --bus-write-range "0000-0003"
            --bus-write-trace "${census}.bus.csv"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(FATAL_ERROR "Upper-table string-record ${suffix} recording failed (${recorder_result}).\n${recorder_output}${recorder_error}")
    endif()
    set(recorder_output "${recorder_output}" PARENT_SCOPE)
endfunction()

record_route("${assembly_census}" assembly)
record_route("${native_census}" native --replace-hook "08:5ABF@all")
set(native_output "${recorder_output}")

foreach(expected_line IN ITEMS
    "Gameplay recording complete: 1800 frames"
    "Native replacement 08:5ABF@all entries: 18 of 18"
    "Work-RAM write trace complete: 646310 selected writes"
    "Bus write trace complete: 1019098 selected writes"
)
    string(FIND "${native_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Upper-table string-record output omitted '${expected_line}'.\n${native_output}")
    endif()
endforeach()

foreach(artifact IN ITEMS census work-ram framebuffer ram bus)
    set(assembly_path "${assembly_census}.${artifact}")
    set(native_path "${native_census}.${artifact}")
    if(artifact STREQUAL "census")
        set(assembly_path "${assembly_census}")
        set(native_path "${native_census}")
    elseif(artifact STREQUAL "work-ram")
        set(assembly_path "${assembly_census}.work-ram.bin")
        set(native_path "${native_census}.work-ram.bin")
    elseif(artifact STREQUAL "framebuffer")
        set(assembly_path "${assembly_census}.framebuffer.rgb")
        set(native_path "${native_census}.framebuffer.rgb")
    elseif(artifact STREQUAL "ram")
        set(assembly_path "${assembly_census}.ram.csv")
        set(native_path "${native_census}.ram.csv")
    elseif(artifact STREQUAL "bus")
        set(assembly_path "${assembly_census}.bus.csv")
        set(native_path "${native_census}.bus.csv")
    endif()
    file(SHA256 "${assembly_path}" assembly_sha256)
    file(SHA256 "${native_path}" native_sha256)
    if(NOT assembly_sha256 STREQUAL native_sha256)
        message(FATAL_ERROR "The native upper-table string record changed ${artifact}.\nAssembly: ${assembly_sha256}\nNative:   ${native_sha256}\n")
    endif()
    string(REPLACE "-" "_" expected_suffix "${artifact}")
    set(expected_sha256 "${expected_${expected_suffix}_sha256}")
    if(NOT assembly_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Upper-table string-record ${artifact} baseline changed.\nExpected: ${expected_sha256}\nActual:   ${assembly_sha256}\n")
    endif()
endforeach()

file(REMOVE
    "${assembly_census}" "${native_census}"
    "${assembly_census}.work-ram.bin" "${native_census}.work-ram.bin"
    "${assembly_census}.framebuffer.rgb" "${native_census}.framebuffer.rgb"
    "${assembly_census}.ram.csv" "${native_census}.ram.csv"
    "${assembly_census}.bus.csv" "${native_census}.bus.csv")

message(STATUS "Semantic 08:5ABF upper-table string records run all 18 entries natively with exact state, RAM, framebuffer and VDC parity.")
