# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported state shuttle configuration: ${CONFIG}")
endif()

# The savestate shuttle, which is the one part of a device-model install that
# the census cannot reach. A recorded route never saves or loads a state, so a
# model could carry a broken shuttle and every census would still match.
#
# The shuttle exists because a model owns state the emulator's savestate
# format is written through. On save the model's values are moved into the
# dormant emulator object before it is serialized; on load they are read back
# out of it afterwards, once the emulator's own repairs have run over them.
# Three of them now: 128 KiB of video memory and register file for the
# controller, the palette and schedule for the colour encoder, and the seven
# fields of the processor's timer and interrupt controller. The last of those
# has no dormant object of its own -- the emulator keeps it as private fields
# of `HuC6280` -- so its shuttle writes through the processor itself.
#
# A routine replay is the cheapest thing in this host that exercises both
# directions for real. It records a route, serializes at the frame before a
# chosen call, deserializes back into it, and re-runs the call -- so a shuttle
# that dropped or corrupted a field would restore a machine that produced
# different work RAM, a different write transcript, or a different cycle
# count, and the replay would refuse it.
#
# Eight of them, and the count is load-bearing. The first version of this test
# replayed once and passed while the encoder's install carried a defect that
# needed six: on every load the emulator's own loader reported a change of
# horizontal timing to the frontend and so did the model, and the frontend's
# sixteen-entry record was overrun by the pair of them. One save and restore
# cannot see a fault that only compounds across several, so the test that
# guards a shuttle has to do more than one round trip.
#
# Eight distinct routines rather than eight calls of one, because the host
# allows a routine only one replay ordinal. Which routines is not the point
# here -- they are the cheapest admitted ones to observe, and what is being
# measured is the eight saves and restores around them.
set(
    replay_routines
    00:F1B4
    00:F241
    00:F4FC
    01:C574
    01:D466
    03:7FB1
    03:7F57
    03:6966
)
#
# Three arms. `emulated` hands the two video devices back and keeps the timer
# and interrupt controller; `emulated_irq` is the other way round. Between them
# every model's shuttle is exercised in one arm and absent from another, which
# is what attributes a restored machine that differs to the device whose
# shuttle lost it.
set(shuttle_arms native emulated emulated_irq)
set(arm_flags_native "")
set(arm_flags_emulated "--emulated-vce;--emulated-vdc")
set(arm_flags_emulated_irq "--emulated-irq")

foreach(arm IN LISTS shuttle_arms)
    set(arm_flags "${arm_flags_${arm}}")

    set(observe_arguments "")
    set(replay_arguments "")
    foreach(routine IN LISTS replay_routines)
        list(APPEND observe_arguments --observe-hook "${routine}")
        list(APPEND replay_arguments --routine-replay "${routine}@1")
    endforeach()

    execute_process(
        COMMAND
            "${APP}"
            --record-gameplay
            "${OUTPUT}.${arm}.csv"
            ${observe_arguments}
            --routine-call-trace
            "${OUTPUT}.${arm}.calls.csv"
            ${replay_arguments}
            ${arm_flags}
            --rom
            "${ROM}"
        RESULT_VARIABLE replay_result
        OUTPUT_VARIABLE replay_output
        ERROR_VARIABLE replay_error
    )
    if(NOT replay_result EQUAL 0)
        message(
            FATAL_ERROR
            "The ${arm} state shuttle replay failed (${replay_result}).\n"
            "${replay_output}${replay_error}"
        )
    endif()

    set(arm_detail "")
    foreach(routine IN LISTS replay_routines)
        string(
            REGEX MATCH
            "Routine replay ${routine}@1 verified: ([^\n]*)"
            replay_line
            "${replay_output}"
        )
        if(NOT replay_line)
            message(
                FATAL_ERROR
                "The ${arm} state shuttle replay did not verify ${routine}. "
                "A device model that loses state across a savestate restores "
                "a machine that cannot reproduce the call it was captured "
                "before.\n"
                "${replay_output}${replay_error}"
            )
        endif()
        string(APPEND arm_detail "  ${routine} ${CMAKE_MATCH_1}\n")
    endforeach()
    set(replay_detail_${arm} "${arm_detail}")
endforeach()

# Verifying in each arm is not enough on its own: the replay compares an arm
# against itself, so three different machines could each be self-consistent.
# The frame, the body cycle count and the write count have to be the same
# ones, which is what says the restored machine is the same machine.
if(NOT replay_detail_native STREQUAL replay_detail_emulated)
    message(
        FATAL_ERROR
        "The restored calls differ between the native and emulated video "
        "devices.\n"
        "  native\n${replay_detail_native}"
        "  emulated\n${replay_detail_emulated}"
    )
endif()
if(NOT replay_detail_native STREQUAL replay_detail_emulated_irq)
    message(
        FATAL_ERROR
        "The restored calls differ between the native and emulated timer and "
        "interrupt controller.\n"
        "  native\n${replay_detail_native}"
        "  emulated\n${replay_detail_emulated_irq}"
    )
endif()

message(
    STATUS
    "Device savestate shuttle verified over eight round trips in each of "
    "three arms:\n"
    "${replay_detail_native}"
)
