# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported native IRQ configuration: ${CONFIG}")
endif()

# The HuC6280's timer and interrupt controller, as a native device model. The
# two register files at $0C00-$0C01 and $1402-$1403, the three request lines
# behind them, and the free-running divider that turns the master clock into
# the one interrupt this machine generates for itself.
#
# This is the narrowest of the four device seams and the only one inside the
# processor rather than beside it. Nothing crosses it -- no framebuffer, no
# sound buffer, no machine edge at all -- and the whole device is under fifty
# bytes of state. What makes it the hardest of the four anyway is where it is
# reached from: every other model was installed at the machine's edge, and this
# one is read from `ADDCYC` and from the top of the processor's instruction
# loop. So the model owns the device and the processor keeps a read-only mirror
# of the three values it reads once per instruction.
#
# What stays with the processor is the taking of an interrupt. Which vector
# wins, when the line is sampled, and how the I flag gates it are decided at
# instruction boundaries, and this device has no notion of an instruction. That
# split is the reason this model can be installed while the processor is still
# the emulator's -- and it is also what this census has to show is intact,
# because a model that published the right level at the wrong moment would move
# every interrupt in the route by an instruction.
#
# The acceptance is that the census does not move at all, from the value the
# sound slice pinned and each video slice reproduced. Same work RAM, same
# framebuffer, same audio, frame for frame, for 3,600 frames.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

# Four arms, and this device's third arm means something the video ones did
# not.
#
# `native` is how the host runs: all four device models installed.
#
# `emulated_irq` hands this one back and keeps the other three, which is the
# arm that attributes a difference to this device rather than to the video
# side it now sits beside.
#
# `native_irq_alone` is the mirror, and it is the arm this seam most needs.
# The three device models beside it are all reached through the machine's I/O
# page or its event handler; this one is reached from inside the processor. Run
# alone against the emulator's own sound and video, it shows that what the
# model publishes is what the emulator's own chips were being answered with --
# not merely that four native models agree with each other.
#
# `emulated_all` hands back all four, which is what makes a matching hash a
# statement about the machine rather than about the models.
set(arm_names native emulated_irq native_irq_alone emulated_all)
set(arm_flags_native "--report-irq-coverage")
set(arm_flags_emulated_irq "--emulated-irq")
set(
    arm_flags_native_irq_alone
    "--emulated-psg;--emulated-vdc;--emulated-vce"
)
set(
    arm_flags_emulated_all
    "--emulated-irq;--emulated-psg;--emulated-vdc;--emulated-vce"
)

foreach(arm IN LISTS arm_names)
    if(arm STREQUAL "native")
        set(arm_output "${OUTPUT}")
    else()
        set(arm_output "${OUTPUT}.${arm}.csv")
    endif()
    set(arm_flags "${arm_flags_${arm}}")

    execute_process(
        COMMAND
            "${APP}"
            --record-state-census
            "${arm_output}"
            ${arm_flags}
            --rom
            "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(
            FATAL_ERROR
            "The ${arm} IRQ recorder failed (${recorder_result}).\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    string(
        FIND
        "${recorder_output}"
        "State census recording complete: 3600 frames"
        line_offset
    )
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "The ${arm} IRQ recording did not complete 3600 frames.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    file(SHA256 "${arm_output}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_census_sha256)
        message(
            FATAL_ERROR
            "The ${arm} IRQ census changed.\n"
            "  expected ${expected_census_sha256}\n"
            "  actual   ${actual_sha256}"
        )
    endif()

    if(arm STREQUAL "native")
        set(native_output "${recorder_output}")
    endif()
endforeach()

# A matching hash proves nothing on its own if the model was never installed,
# because the emulator would have answered and produced exactly this hash.
if(NOT native_output MATCHES "Timer and interrupt controller paths:")
    message(
        FATAL_ERROR
        "The native arm reported no timer and interrupt controller coverage, "
        "which means the emulator answered for the device and the matching "
        "hash proves nothing.\n${native_output}"
    )
endif()

# A route's worth of counting down. Two hundred thousand underflows is not a
# threshold this device could pass by accident: it is the timer interrupt this
# machine runs its music and its scheduler on, arriving several hundred times a
# second for an hour of gameplay.
string(
    REGEX MATCH
    "  timer: ([0-9]+) underflows, ([0-9]+) reloads"
    coverage_timer
    "${native_output}"
)
if(CMAKE_MATCH_1 LESS 200000)
    message(
        FATAL_ERROR
        "The native controller counted only ${CMAKE_MATCH_1} underflows, "
        "which is not a route's worth of timer interrupts.\n${native_output}"
    )
endif()
if(NOT CMAKE_MATCH_1 EQUAL CMAKE_MATCH_2)
    message(
        FATAL_ERROR
        "The native controller underflowed ${CMAKE_MATCH_1} times and "
        "reloaded ${CMAKE_MATCH_2}; every underflow is followed by exactly "
        "one reload one tick later.\n${native_output}"
    )
endif()

# And the other line that crosses this boundary. The raster interrupt is
# raised by the native colour encoder and lowered through the same two entry
# points, so a non-zero count here is the one place in the census where two
# device models are shown talking to each other through the processor.
string(
    REGEX MATCH
    "  lines raised: ([0-9]+) irq2, ([0-9]+) irq1, ([0-9]+) timer"
    coverage_lines
    "${native_output}"
)
if(CMAKE_MATCH_2 LESS 3600)
    message(
        FATAL_ERROR
        "The native controller saw only ${CMAKE_MATCH_2} video interrupts "
        "over 3,600 frames.\n${native_output}"
    )
endif()

# What the route does not reach is again the more useful half, and for this
# device it is a different shape of gap from the video chips'. Devil's Crush
# leans on the timer constantly and on the interrupt register file barely at
# all: it never reads the counter back, never writes or reads the mask -- the
# mask is opened once by the reset vector and never narrowed -- and never reads
# the status register, acknowledging the timer by writing $1403 instead. There
# is no compact disc unit, so IRQ2 is never asserted. And the timer interrupt
# horizon is never queried, because that entry point exists for native routine
# transcripts and a census route installs none; the replacement tests are what
# drive it.
#
# Those paths are the ones `dc_irq_device` enumerates against the emulator's
# own controller. This census cannot fail for a path it never takes, and
# printing the counts keeps that division of labour visible rather than
# implied.
string(
    REGEX MATCH
    "Timer and interrupt controller paths:[^\n]*\n(  [^\n]*\n)+"
    coverage_block
    "${native_output}"
)
message(
    STATUS
    "What the route asked of the timer and interrupt controller:\n"
    "${coverage_block}"
)

message(
    STATUS
    "Native IRQ device model verified: four arms reproduce "
    "${expected_census_sha256}"
)
