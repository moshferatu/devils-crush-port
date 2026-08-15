# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported palette-cycle configuration: ${CONFIG}")
endif()

# 09:60C7-09:612A, the palette-block colour rotator, and bank 09's first
# registered routine of any kind. Fifty-nine instructions, 996 calls, no branch
# anywhere in it: it takes a block number in the accumulator, rotates colours
# three, four and five of that block of the palette shadow at $27B9, and sets
# the block's dirty flag at $2CCC,Y and the shadow's own at $2CCB.
#
# **The oracle is every byte the routine touches, compared rather than
# digested.** Three ranges, and each is there for a different claim:
#
#   $2028-$2032   the zero-page scratch -- <$2B, <$2D/<$2E and the pointer the
#                 routine builds in <$2F/<$30. This is where the five
#                 `ASL A`/`ROL <$30` pairs and the two `ADC`s land, so a
#                 mis-modelled carry between the halves of `$27B9 + block * 32`
#                 shows up here before it can reach a colour.
#   $27B9-$2FB8   the palette shadow, all thirty-two blocks. The rotation
#                 itself: six bytes moved per call, and a wrong `Y` at any of
#                 the six `STA [$2F],Y` writes the right byte to the wrong
#                 colour.
#   $2CCB-$2CEB   the dirty flags the whole-palette `TIA` at 00:EB46 reads.
#                 `$2CCC,Y` is indexed by the block number the `PHA`/`PLA` pair
#                 carries across the body, so this range is what proves the
#                 stack round-trip.
#
# It is a comparison rather than a per-routine digest for the reason the rest of
# the suite is: three times now a transcript has named the wrong cycle for a
# write while a pinned hash agreed with it, and each time what found it was
# requiring a replaced run's writes to match an emulated one's. The trace
# carries the master-cycle offset of every write, so this pins the routine's
# timing as well as its arithmetic.
#
# The census is pinned on top, because a comparison cannot see route drift --
# both sides would drift together.
#
# The call trace is the third artifact and it is here for one thing only: this
# transcript ends *past* its `RTS` at 612A rather than in front of it, and an
# absorbed return deletes the instruction boundary the routine-call observer
# closes a call on. Requiring the emulated and replaced traces to be identical
# is what proves the declaration put the boundary back where the assembly had
# it -- same 996 calls, same return address, same body_master_cycles.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

set(emulated_census "${OUTPUT}.emulated.csv")
set(emulated_writes "${emulated_census}.writes.csv")
set(replaced_census "${OUTPUT}.replaced.csv")
set(replaced_writes "${replaced_census}.writes.csv")
set(emulated_calls "${OUTPUT}.emulated.calls.csv")
set(replaced_calls "${OUTPUT}.replaced.calls.csv")

function(record_route census writes calls)
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${census}"
            --work-ram-write-range 2028-2032
            --work-ram-write-range 27B9-2FB8
            --work-ram-write-range 2CCB-2CEB
            --work-ram-write-trace "${writes}"
            --observe-hook "09:60C7"
            --routine-call-trace "${calls}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(
            FATAL_ERROR
            "Semantic palette-cycle recorder failed (${recorder_result}).\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    set(recorder_output "${recorder_output}" PARENT_SCOPE)
endfunction()

record_route("${emulated_census}" "${emulated_writes}" "${emulated_calls}")
set(emulated_output "${recorder_output}")

record_route(
    "${replaced_census}" "${replaced_writes}" "${replaced_calls}"
    --replace-hook "09:60C7@all"
)
set(replaced_output "${recorder_output}")

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 09:60C7@all entries: 996 of 996"
    "Observe hook 1 entries: 996"
    "Routine call trace complete: 996 calls, 24175 writes"
)
    string(FIND "${replaced_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic palette-cycle output omitted '${expected_line}'.\n"
            "${replaced_output}"
        )
    endif()
endforeach()

# A run that recorded nothing would compare equal to another that recorded
# nothing, so the emulated side is required to have traced the shadow too.
string(
    REGEX MATCH
    "Work-RAM write trace complete: ([0-9]+) selected writes"
    emulated_count_line
    "${emulated_output}"
)
if(emulated_count_line STREQUAL "" OR CMAKE_MATCH_1 LESS 2000000)
    message(
        FATAL_ERROR
        "The emulated route did not trace the palette shadow.\n"
        "${emulated_output}"
    )
endif()
set(shadow_write_count "${CMAKE_MATCH_1}")

foreach(artifact IN ITEMS census write call)
    if(artifact STREQUAL "census")
        set(emulated_path "${emulated_census}")
        set(replaced_path "${replaced_census}")
    elseif(artifact STREQUAL "write")
        set(emulated_path "${emulated_writes}")
        set(replaced_path "${replaced_writes}")
    else()
        set(emulated_path "${emulated_calls}")
        set(replaced_path "${replaced_calls}")
    endif()
    file(SHA256 "${emulated_path}" emulated_sha256)
    file(SHA256 "${replaced_path}" replaced_sha256)
    if(NOT emulated_sha256 STREQUAL replaced_sha256)
        message(
            FATAL_ERROR
            "The replaced palette cycle's ${artifact} differs from the "
            "emulated route.\nEmulated: ${emulated_sha256}\n"
            "Replaced: ${replaced_sha256}\n"
            "Re-record both and diff them; a single differing line names the "
            "instruction, the byte and the cycle the transcript got wrong, "
            "and a whole trace shifted by a constant names an instruction "
            "priced wrong."
        )
    endif()
endforeach()

file(SHA256 "${emulated_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(
        FATAL_ERROR
        "Semantic palette-cycle census artifact changed.\n"
        "Expected: ${expected_census_sha256}\nActual:   ${census_sha256}\n"
    )
endif()

file(REMOVE
    "${emulated_census}" "${emulated_writes}" "${emulated_calls}"
    "${emulated_calls}.writes.csv"
    "${replaced_census}" "${replaced_writes}" "${replaced_calls}"
    "${replaced_calls}.writes.csv")

message(
    STATUS
    "The palette-block colour rotator runs natively on all 996 calls, and the "
    "route's ${shadow_write_count} writes to the palette shadow, its dirty "
    "flags and the pointer scratch land on the same cycles as assembly."
)
