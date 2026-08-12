# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "Unsupported native instruction boundary configuration: ${CONFIG}"
    )
endif()

# The decision a HuC6280 makes between two instructions, as a native model:
# the two values it latches at the end of an instruction, and the rule that
# decides at the start of the next one whether the next thing to happen is the
# next instruction or an interrupt.
#
# This is the sixth device seam and the first that is a rule rather than a
# device. The five before it each replaced something with a state boundary
# around it; this one holds two values and otherwise computes, so it hands
# over no storage, leaves no mirror, and needs nothing kept coherent with
# anything. The processor passes in the request register, the mask and its own
# I flag -- three values it already holds -- and takes back which of the four
# vectors won.
#
# `dc_interrupt_boundary` already proved the rule by enumeration: 1,024
# latches and 65,536 decisions, every input the boundary has. What that test
# cannot say is whether the machine reaches the model at all, or with the
# values it would have used. That is this test's half of the argument, and the
# acceptance for it is that the census does not move -- same work RAM, same
# framebuffer, same audio, frame for frame, for 3,600 frames, from the value
# the sound slice pinned and every slice since has reproduced.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

# Four arms, on the same terms as the five seams before this one.
#
# `native` is how the host runs: all six models installed.
#
# `emulated_boundary` hands this one back and keeps the other five, which is
# the arm that attributes a difference to this rule rather than to the five it
# now sits among.
#
# `native_boundary_alone` is the arm this seam most needs, and what it asks is
# sharper than a device's counterpart. Every instruction in the route passes
# through this decision twice, under the emulator's own sound, video,
# interrupt controller and clock -- so a rule that differed on any single one
# of the 221 million of them would take a vector the processor would not have
# taken, or fail to take one it would, and the frame it happened on and every
# frame after it would differ.
#
# `emulated_all` hands back all six, which is what makes a matching hash a
# statement about the machine rather than about the models.
set(
    arm_names
    native
    emulated_boundary
    native_boundary_alone
    emulated_all
)
set(arm_flags_native "--report-boundary-coverage")
set(arm_flags_emulated_boundary "--emulated-boundary")
set(
    arm_flags_native_boundary_alone
    "--emulated-psg;--emulated-vdc;--emulated-vce;--emulated-irq"
    "--emulated-scheduler"
)
set(
    arm_flags_emulated_all
    "--emulated-boundary"
    "--emulated-psg;--emulated-vdc;--emulated-vce;--emulated-irq"
    "--emulated-scheduler"
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
            "The ${arm} boundary recorder failed (${recorder_result}).\n"
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
            "The ${arm} boundary recording did not complete 3600 frames.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    file(SHA256 "${arm_output}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_census_sha256)
        message(
            FATAL_ERROR
            "The ${arm} boundary census changed.\n"
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
if(NOT native_output MATCHES "Instruction boundary paths:")
    message(
        FATAL_ERROR
        "The native arm reported no instruction boundary coverage, which "
        "means the emulator answered for the rule and the matching hash "
        "proves nothing.\n${native_output}"
    )
endif()

# A route's worth of instructions. Both counters run once per instruction, so
# sixty million is not a threshold a model that was installed but rarely
# reached could pass.
#
# The floor was a hundred million until the frame-sync spin stopped being
# executed. The machine now advances through `00:E323` instead of running it a
# turn at a time, so the route makes 66,577,225 decisions where it used to make
# 108,732,259 -- a difference of 42,155,034, which is exactly two instructions
# for each of the 21,077,517 iterations skipped. Nothing else about this
# boundary moved: the vector counts below, the latched and masked sample
# counts, and the declined decisions are all identical either way, and so is
# the census hash. Passing `--emulated-idle` restores the old numbers.
string(
    REGEX MATCH
    "  decisions: ([0-9]+), instruction ([0-9]+) \\(([0-9]+) woken"
    coverage_decisions
    "${native_output}"
)
# Without this the checks below would read whatever the last successful match
# left in `CMAKE_MATCH_1`, so a report whose wording drifted would pass rather
# than fail.
if(NOT coverage_decisions)
    message(
        FATAL_ERROR
        "The native arm's boundary coverage did not report a decision count "
        "in the expected form.\n${native_output}"
    )
endif()
if(CMAKE_MATCH_1 LESS 60000000)
    message(
        FATAL_ERROR
        "The native boundary made only ${CMAKE_MATCH_1} decisions, which is "
        "not a route's worth of instructions.\n${native_output}"
    )
endif()

# And the decisions that were not instructions, which is where the rule lives.
# A boundary that admitted no vector at all over an hour of frames would be
# indistinguishable from one that always answered `instruction` -- and this
# machine generates an interrupt for itself every few hundred instructions, so
# the number is large rather than merely non-zero.
string(
    REGEX MATCH
    "  vectors: ([0-9]+) reset, ([0-9]+) timer \\(([0-9]+) on a line"
    coverage_vectors
    "${native_output}"
)
if(NOT coverage_vectors)
    message(
        FATAL_ERROR
        "The native arm's boundary coverage did not report vector counts in "
        "the expected form.\n${native_output}"
    )
endif()
if(CMAKE_MATCH_2 LESS 1)
    message(
        FATAL_ERROR
        "The native boundary admitted no timer vector over 3600 frames, "
        "which means the route never asked it the only question this "
        "machine asks.\n${native_output}"
    )
endif()

# The asymmetry, as a number rather than an argument. The timer line is tested
# live where IRQ1 and IRQ2 are tested against the latch, so a timer request
# raised after the latch and before the decision is taken immediately. A route
# that never reached that would have been served just as well by a rule that
# tested the sample -- so this is the one counter here whose being non-zero is
# a claim about the emulator's rule and not only about the model's.
if(CMAKE_MATCH_3 LESS 1)
    message(
        FATAL_ERROR
        "The native boundary never took a timer vector on a line its latch "
        "did not hold, so the census says nothing about the one asymmetry "
        "this rule was separated for.\n${native_output}"
    )
endif()

# What the route does not reach, printed rather than asserted, because for
# this seam the gaps are the interesting part and they are all in one place.
# The reset line is signalled twice and acknowledged once, which is the whole
# of a machine's life: `Reset` runs for the power-on and again for the ROM
# load, and only one of the two is followed by a processor that takes the
# vector. IRQ2 is never raised at all -- it belongs to the compact disc unit,
# which this cartridge does not have. The enumeration in
# `dc_interrupt_boundary` is what covers the rest, including the states no
# machine can be driven into.
string(
    REGEX MATCH
    "Instruction boundary paths:[^\n]*\n(  [^\n]*\n)+"
    coverage_block
    "${native_output}"
)
message(
    STATUS
    "What the route asked of the instruction boundary:\n"
    "${coverage_block}"
)

message(
    STATUS
    "Native instruction boundary verified: four arms reproduce "
    "${expected_census_sha256}"
)
