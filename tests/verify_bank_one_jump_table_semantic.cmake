# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported jump-table configuration: ${CONFIG}")
endif()

# 01:C000-01:C0CB, bank 01's jump table: sixty-eight `JMP absolute` rows that the
# rest of the cartridge reaches bank 01's routines by, of which this route walks
# eighteen. One instruction each, twelve master cycles, no register, no flag, no
# read and no write -- the shortest transcript in the port.
#
#   C012 -> $F08E   C02A -> $F54F   C063 -> $D6E2   C081 -> $E8B3
#   C018 -> $C5C9   C03C -> $E93C   C069 -> $D75E   C087 -> $ECFD
#   C021 -> $C5EA   C042 -> $E1FD   C06C -> $F4FC   C09F -> $D8CA
#   C024 -> $C5AB   C05D -> $EBA5   C06F -> $DAF8   C0A2 -> $D8D2
#   C027 -> $C574   C060 -> $E241
#
# **The counts below are arrivals with nothing else replaced, and they are not
# the profile's.** The instruction profile is recorded with the registry
# deployed, where a row reached inside a neighbouring transcript is not an
# interpreted execution at all: 8,541 arrivals here against 8,446 emulated
# instructions there. The ninety-five in between are three rows other families
# already walk in line -- 01:C018 ninety-one times, 01:C02A three and 01:C021
# once. Both numbers are right about different questions, and this file asks the
# first one, because a hook run is what admits a routine.
#
# **The oracle is the stack page, and the reason is that this family writes
# nothing.** There is no output range of its own to compare: a row moves the
# program counter and charges twelve cycles, and that is the whole of it. But a
# cycle miscounted here shifts every subsequent instruction on the route, and
# $2100-21FF records the cycle of every push and pop the route makes -- four
# million of them -- so the whole trace moves if any one of the eighteen rows is
# priced wrong. Eleven of the eighteen jump straight into a routine that pushes
# immediately, which is as close to the seam as this range gets.
#
# It is a comparison rather than a digest for the reason the rest of the suite
# is: three times now a transcript has named the wrong cycle for a write while a
# pinned per-routine hash agreed with it, and each time what found it was
# requiring a replaced run's writes to match an emulated one's.
#
# The census is pinned as well, because a comparison cannot see route drift --
# both sides would drift together -- and because the census is what a wrong
# *target* would move: a row that jumped to the wrong address would not shift
# the stack page, it would end the route.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

set(emulated_census "${OUTPUT}.emulated.csv")
set(emulated_writes "${emulated_census}.writes.csv")
set(replaced_census "${OUTPUT}.replaced.csv")
set(replaced_writes "${replaced_census}.writes.csv")

function(record_route census writes)
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${census}"
            --work-ram-write-range 2100-21FF
            --work-ram-write-trace "${writes}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(
            FATAL_ERROR
            "Semantic jump-table recorder failed (${recorder_result}).\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    set(recorder_output "${recorder_output}" PARENT_SCOPE)
endfunction()

record_route("${emulated_census}" "${emulated_writes}")
set(emulated_output "${recorder_output}")

record_route(
    "${replaced_census}" "${replaced_writes}"
    --replace-hook "01:C012@all"
    --replace-hook "01:C018@all"
    --replace-hook "01:C021@all"
    --replace-hook "01:C024@all"
    --replace-hook "01:C027@all"
    --replace-hook "01:C02A@all"
    --replace-hook "01:C03C@all"
    --replace-hook "01:C042@all"
    --replace-hook "01:C05D@all"
    --replace-hook "01:C060@all"
    --replace-hook "01:C063@all"
    --replace-hook "01:C069@all"
    --replace-hook "01:C06C@all"
    --replace-hook "01:C06F@all"
    --replace-hook "01:C081@all"
    --replace-hook "01:C087@all"
    --replace-hook "01:C09F@all"
    --replace-hook "01:C0A2@all"
)
set(replaced_output "${recorder_output}")

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:C012@all entries: 924 of 924"
    "Native replacement 01:C018@all entries: 1599 of 1599"
    "Native replacement 01:C021@all entries: 71 of 71"
    "Native replacement 01:C024@all entries: 44 of 44"
    "Native replacement 01:C027@all entries: 123 of 123"
    "Native replacement 01:C02A@all entries: 40 of 40"
    "Native replacement 01:C03C@all entries: 204 of 204"
    "Native replacement 01:C042@all entries: 1119 of 1119"
    "Native replacement 01:C05D@all entries: 1119 of 1119"
    "Native replacement 01:C060@all entries: 996 of 996"
    "Native replacement 01:C063@all entries: 7 of 7"
    "Native replacement 01:C069@all entries: 3 of 3"
    "Native replacement 01:C06C@all entries: 14 of 14"
    "Native replacement 01:C06F@all entries: 2139 of 2139"
    "Native replacement 01:C081@all entries: 78 of 78"
    "Native replacement 01:C087@all entries: 58 of 58"
    "Native replacement 01:C09F@all entries: 2 of 2"
    "Native replacement 01:C0A2@all entries: 1 of 1"
)
    string(FIND "${replaced_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic jump-table output omitted '${expected_line}'.\n"
            "${replaced_output}"
        )
    endif()
endforeach()

# A run that recorded nothing would compare equal to another that recorded
# nothing, so the emulated side is required to have traced the stack page too.
string(
    REGEX MATCH
    "Work-RAM write trace complete: ([0-9]+) selected writes"
    emulated_count_line
    "${emulated_output}"
)
if(emulated_count_line STREQUAL "" OR CMAKE_MATCH_1 LESS 1000000)
    message(
        FATAL_ERROR
        "The emulated route did not trace the stack page.\n${emulated_output}"
    )
endif()
set(stack_write_count "${CMAKE_MATCH_1}")

foreach(artifact IN ITEMS census write)
    if(artifact STREQUAL "census")
        set(emulated_path "${emulated_census}")
        set(replaced_path "${replaced_census}")
    else()
        set(emulated_path "${emulated_writes}")
        set(replaced_path "${replaced_writes}")
    endif()
    file(SHA256 "${emulated_path}" emulated_sha256)
    file(SHA256 "${replaced_path}" replaced_sha256)
    if(NOT emulated_sha256 STREQUAL replaced_sha256)
        message(
            FATAL_ERROR
            "The replaced jump table's ${artifact} differs from the emulated "
            "route.\nEmulated: ${emulated_sha256}\n"
            "Replaced: ${replaced_sha256}\n"
            "Re-record both and diff them; a single differing line names the "
            "instruction, the byte and the cycle the transcript got wrong, and "
            "a whole trace shifted by a constant names a row priced wrong."
        )
    endif()
endforeach()

file(SHA256 "${emulated_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(
        FATAL_ERROR
        "Semantic jump-table census artifact changed.\n"
        "Expected: ${expected_census_sha256}\nActual:   ${census_sha256}\n"
    )
endif()

file(REMOVE
    "${emulated_census}" "${emulated_writes}"
    "${replaced_census}" "${replaced_writes}")

message(
    STATUS
    "The eighteen live rows of bank 01's jump table run natively on all 8,541 "
    "arrivals, and the route's ${stack_write_count} stack-page writes land on "
    "the same cycles as assembly."
)
