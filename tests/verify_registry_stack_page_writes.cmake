# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "The registry stack-page regression is Release-only.")
endif()

# The state census cannot see the stack page. It samples work RAM at frame
# boundaries, and a byte a subroutine pushes has been popped again long before
# the frame ends, so a transcript can name the wrong cycle for a push -- or
# push the wrong byte outright, which is what an interrupt taken at a mistaken
# boundary would do -- and every census in this suite will agree with it. Twice
# now a per-write cycle has been wrong under a pinned hash that agreed with the
# mistake, and both times what found it was this comparison rather than a
# constant: run the whole route emulated, run it again with every admitted
# routine deployed, and require the two to have written the same bytes to
# $2100-21FF at the same cycles, in the same order.
#
# It is deliberately not a pinned digest. A digest states that the registry
# still does what it did; this states that the registry still does what the
# original does, which is the property the port is actually claiming, and it
# cannot quietly come to agree with a defect.
set(emulated_trace "${OUTPUT}.emulated.writes.csv")
set(registry_trace "${OUTPUT}.registry.writes.csv")
set(emulated_census "${OUTPUT}.emulated.census.csv")
set(registry_census "${OUTPUT}.registry.census.csv")

function(record_route census trace)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${census}"
            --work-ram-write-range 2100-21FF
            --work-ram-write-trace "${trace}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(
            FATAL_ERROR
            "Stack-page recorder failed (${recorder_result}).\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    set(recorder_output "${recorder_output}" PARENT_SCOPE)
endfunction()

record_route("${emulated_census}" "${emulated_trace}")
set(emulated_output "${recorder_output}")
record_route("${registry_census}" "${registry_trace}" --replace-registry)
set(registry_output "${recorder_output}")

# A run that recorded nothing would compare equal to another that recorded
# nothing, so the count is read back before the traces are.
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

# The registry has to reach the same routines, not merely avoid disagreeing
# with the ones it reached, so its own summary is read back too.
string(
    REGEX MATCH
    "Registry calls: ([0-9]+) native of ([0-9]+) entered"
    registry_calls_line
    "${registry_output}"
)
if(registry_calls_line STREQUAL "" OR CMAKE_MATCH_1 LESS 1)
    message(
        FATAL_ERROR
        "The registry route executed no admitted routine natively.\n"
        "${registry_output}"
    )
endif()
set(native_calls "${CMAKE_MATCH_1}")

foreach(artifact IN ITEMS census trace)
    if(artifact STREQUAL "census")
        set(emulated_path "${emulated_census}")
        set(registry_path "${registry_census}")
    else()
        set(emulated_path "${emulated_trace}")
        set(registry_path "${registry_trace}")
    endif()
    file(SHA256 "${emulated_path}" emulated_sha256)
    file(SHA256 "${registry_path}" registry_sha256)
    if(NOT emulated_sha256 STREQUAL registry_sha256)
        message(
            FATAL_ERROR
            "The registry's ${artifact} differs from the emulated route.\n"
            "Emulated: ${emulated_sha256}\nRegistry: ${registry_sha256}\n"
            "Re-record both with --work-ram-write-trace and diff them; a "
            "single differing line names the instruction, the byte and the "
            "cycle that the transcript got wrong."
        )
    endif()
endforeach()

file(REMOVE
    "${emulated_trace}" "${registry_trace}"
    "${emulated_census}" "${registry_census}")

message(
    STATUS
    "The registry writes the stack page exactly as the original does: "
    "${stack_write_count} writes over $2100-21FF, same bytes at the same "
    "cycles, across ${native_calls} native calls."
)
