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
# test of its own rather than a longer one. Six are registered.
#
# It also takes the routine to watch, which is usually the same one and for the
# last is not. A routine entered by an `RTS` landing rather than by a `JSR`, and
# returning above the pointer it was entered with, is not a call the observer
# can open and close: it opens one at the entry pointer and closes it at the
# next `RTS` that stands there, which is some ancestor's, so the call is
# reported with that ancestor's writes. Watching such a routine measures the
# instrument rather than the routine. The call its return actually closes was
# opened by the head of the same wrapper, and the head is what OBSERVE names.
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
#
# 02:58C2 is the Sorcerer tick's main piece, and it is the third shape: a walk
# that runs a call in line and then ends past its own return. Its transcript
# contains the collision-table interpreter's whole body, including the `JSR`
# frame it stages in the stack page three bytes below where the absorbed return
# reads -- so the two uses of that page have to stay out of each other's way,
# and neither of the routines above stages anything at all.
#
# 03:6966 is that interpreter, and it is the fourth shape: a routine whose
# return belongs to the entry rather than to the routine. The same function
# serves the registered entry and three sites that run it in line, and those
# three read the `RTS` it stopped on so that they can transcribe the return
# themselves, so only the entry the core takes runs past it. It is also the
# busiest routine on the census route -- 54,149 calls with nothing else
# replaced -- which makes it the one that would show a mismatch soonest.
#
# 01:C5C9 is the coroutine scheduler, and it is the fifth shape: a return whose
# frame is not above the pointer the routine was entered with. Its first two
# instructions are `PLA`s that take its own return address off the stack so it
# can record where it was called from, and the `RTS` it ends on then reads the
# frame above that -- its caller's caller's. Every other absorption in the
# registry reads $2100+S+1 and +2; this one reads +3 and +4. Reading the wrong
# pair here would not look wrong: it would return to the address the routine was
# called from, which is a real address that the machine would go on executing.
#
# 00:ECE2 is the VBlank wrapper's tail, and it is the sixth shape: a return that
# closes a call another routine opened. 00:ECC6 saves three MPR shadows and
# stops in front of `JSR $ECF2`; the core makes the call, the sequencer runs,
# and control comes back not to 00:ECC6 but to 00:ECE2, which is a registered
# routine in its own right. That transcript pulls the three shadows back and
# returns -- three bytes above where it was entered, and on the frame 00:ECC6's
# caller pushed. So the return being absorbed here belongs to a call that was
# opened two transcripts earlier by a different registered routine, which is
# why this one is watched at 00:ECC6. Watched at itself it does not finish:
# the call stays open for the rest of the route and the write trace passed
# twenty gigabytes before it was stopped.
#
# It is also the only absorbed return that happens in interrupt context.
#
# 03:601E is the Tower-insect controller's tail, and it is the seventh shape:
# the same handover as above, but between three transcripts of one routine
# rather than between two wrappers. 03:6016 crosses its `JSR` into the
# collision interpreter and 03:601C crosses its `BSR` into the animation leaf,
# so the call the observer opened at 03:6016 is closed by an `RTS` two
# transcripts later, at an address the machine only ever arrives at from a
# callee's return. It is watched at 03:6016 for the same reason 00:ECE2 is
# watched at 00:ECC6.
#
# Seven of its 6,494 arrivals are hits and decline, so this is also the only
# one of the seven where the replacement is not installed on every call -- the
# comparison has to hold with the two paths interleaved.

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT ROUTINE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

# Watching the routine itself is the usual case and the default.
if(NOT DEFINED OBSERVE OR "${OBSERVE}" STREQUAL "")
    set(OBSERVE "${ROUTINE}")
endif()

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
elseif(routine STREQUAL "02:58C2")
    set(expected_entries
        "Native replacement 02:58C2@all entries: 18152 of 18152")
    set(expected_trace
        "Routine call trace complete: 18152 calls, 321875 writes")
elseif(routine STREQUAL "03:6966")
    set(expected_entries
        "Native replacement 03:6966@all entries: 54149 of 54149")
    set(expected_trace
        "Routine call trace complete: 54149 calls, 542826 writes")
elseif(routine STREQUAL "01:C5C9")
    set(expected_entries
        "Native replacement 01:C5C9@all entries: 2462 of 2462")
    set(expected_trace
        "Routine call trace complete: 2462 calls, 863105 writes")
elseif(routine STREQUAL "00:ECE2")
    set(expected_entries
        "Native replacement 00:ECE2@all entries: 3599 of 3599")
    set(expected_trace
        "Routine call trace complete: 3599 calls, 951407 writes")
elseif(routine STREQUAL "03:601E")
    set(expected_entries
        "Native replacement 03:601E@all entries: 6487 of 6494")
    set(expected_trace
        "Routine call trace complete: 6494 calls, 114100 writes")
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
        "${OBSERVE}"
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
    "Absorbing the return at ${routine} leaves every call observed at "
    "${OBSERVE} reported exactly as the emulated routine reports it."
)
