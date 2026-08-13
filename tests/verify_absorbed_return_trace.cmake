# SPDX-License-Identifier: GPL-3.0-or-later

# A transcript that ends past its own RTS has to be invisible to the instrument
# that watches for one.
#
# The routine-call observer completes a call on an emulated RTS boundary at the
# frame the call was entered on. Absorbing the return deletes that boundary, and
# the first version of this technique deleted it without telling anyone: calls
# at 00:E93C stayed open until an unrelated ancestor returned at the same stack
# pointer, and were then reported with that ancestor's address, its cycle and
# every write made in between -- 38,057,951 writes across 8,966 calls, where the
# routine makes 12,905 across 8,969.
#
# So the transaction declares the return it absorbs, and the host holds the call
# open until the machine is demonstrably past it. There are two ways to be past
# it. Usually the transcript resumes at the caller and the next boundary stands
# there. Sometimes an interrupt is delivered at the end of the transcript and
# takes that address straight to the vector, in which case the machine never
# stands on it and the return is recognised from the frame the interrupt just
# pushed. Both are measured back to the boundary the RTS would have occupied,
# and the three bytes of the interrupt's own frame are not the call's writes.
#
# This test is the proof, and it is a comparison rather than a constant: the
# same route, traced with the routine emulated and with it replaced, has to
# produce the same trace. Nothing here is a recorded expectation that could
# drift into agreeing with a mistake -- the emulated run is the answer.
#
# The script takes the routine to prove, so that each one it is pointed at is a
# test of its own rather than a longer one. Two are registered.
#
# 00:E93C is the palette family's most-called walk and the one the technique was
# established on. It returns to more than one caller, it is called from more
# than one bank, and 54 of its calls are interrupted at exactly the point that
# distinguishes the two recognitions above, so it exercises both.
#
# 08:40DF is the sprite emitter, and it is here because it is the case neither
# of those recognitions got right on its own. Its transcript is long enough to
# be suspended by an interrupt part-way through and then interrupted again at
# its end, on 58 of its 73,818 calls. The second recognition then measured the
# return by the transcript's own reckoning, which was made when the transaction
# was installed and knows nothing of the handler that ran inside it, and
# reported a return 729 cycles early with the last entry's writes missing. The
# palette family is never suspended, so no run of the test above could fail for
# this.

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT ROUTINE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

# The counts are written out per routine rather than passed in, so that the
# admitted-registry generator can read this file and see which routines it
# proves. It matches on the console line, and an interpolated count is not one.
set(routine "${ROUTINE}")
if(routine STREQUAL "00:E93C")
    set(expected_entries
        "Native replacement 00:E93C@all entries: 8969 of 8969")
    set(expected_trace "Routine call trace complete: 8969 calls, 12905 writes")
elseif(routine STREQUAL "08:40DF")
    set(expected_entries
        "Native replacement 08:40DF@all entries: 73818 of 73818")
    set(expected_trace
        "Routine call trace complete: 73818 calls, 1477731 writes")
else()
    message(FATAL_ERROR "Unknown absorbed-return routine: ${routine}")
endif()

function(dc_trace_route label extra_argument output_variable)
    set(census "${OUTPUT}.${label}.census.csv")
    set(calls "${OUTPUT}.${label}.calls.csv")
    set(command
        "${APP}"
        --record-state-census
        "${census}"
        --observe-hook
        "${routine}"
    )
    if(NOT extra_argument STREQUAL "")
        list(APPEND command --replace-hook "${extra_argument}")
    endif()
    list(APPEND command --routine-call-trace "${calls}" --rom "${ROM}")

    execute_process(
        COMMAND ${command}
        RESULT_VARIABLE trace_result
        OUTPUT_VARIABLE trace_output
        ERROR_VARIABLE trace_error
    )
    if(NOT trace_result EQUAL 0)
        message(
            FATAL_ERROR
            "Absorbed-return ${label} run failed (${trace_result}).\n"
            "${trace_output}${trace_error}"
        )
    endif()

    file(SHA256 "${census}" census_sha256)
    file(SHA256 "${calls}" calls_sha256)
    file(SHA256 "${calls}.writes.csv" writes_sha256)
    set(${output_variable}
        "${census_sha256} ${calls_sha256} ${writes_sha256}"
        PARENT_SCOPE
    )
    set(${output_variable}_console "${trace_output}" PARENT_SCOPE)
endfunction()

dc_trace_route(emulated "" emulated_hashes)
dc_trace_route(replaced "${routine}@all" replaced_hashes)

if(NOT emulated_hashes STREQUAL replaced_hashes)
    message(
        FATAL_ERROR
        "Absorbing the return at ${routine} changed what the route observes.\n"
        "Emulated (census calls writes): ${emulated_hashes}\n"
        "Replaced (census calls writes): ${replaced_hashes}\n"
    )
endif()

# The comparison above would also pass if the replacement never ran, so pin the
# fact that it did.
foreach(expected_line IN ITEMS "${expected_entries}" "${expected_trace}")
    string(FIND "${replaced_hashes_console}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Absorbed-return replaced run omitted '${expected_line}'.\n"
            "${replaced_hashes_console}"
        )
    endif()
endforeach()

message(
    STATUS
    "Absorbing the return at ${routine} leaves every observed call reported "
    "exactly as the emulated routine reports it."
)
