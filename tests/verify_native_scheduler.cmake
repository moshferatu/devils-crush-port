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
        "Unsupported native scheduler configuration: ${CONFIG}"
    )
endif()

# The HuC6280's clock and event scheduler, as a native device model. The
# master-cycle timestamp every device in this machine measures itself against,
# the two horizons that decide when the processor stops to let something else
# happen, the overclock divider, the rate a processor cycle is priced at, and
# the loop flag that ends a frame.
#
# This is the fifth device seam and the first whose shape is a block of memory
# rather than a call table. Every model before it replaced a device the machine
# reached at its own edge; this one holds values the processor reads from
# inside its instruction loop -- the clock on every memory write and every
# replacement offer, the near horizon in the one opcode that waits, the run
# flag in the loop condition itself. So the model hands the processor its
# register block and the processor reads it directly, while every change to it
# is still a call on the model.
#
# What stays with the processor is execution. Fetching an opcode and deciding
# at an instruction boundary that an interrupt has arrived need to know what an
# instruction is; a cycle costing three master clocks or twelve, an event
# handler coming due, and a frame boundary wrapping the clock do not.
#
# The acceptance is that the census does not move at all, from the value the
# sound slice pinned and every slice since has reproduced. Same work RAM, same
# framebuffer, same audio, frame for frame, for 3,600 frames -- which for this
# device is a stronger statement than it was for any of the four before it,
# because a scheduler that was wrong by one master clock anywhere would not
# merely change one device's output but would move every device in the machine
# relative to every other.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

# Four arms, on the same terms as the four seams before this one.
#
# `native` is how the host runs: all five device models installed.
#
# `emulated_scheduler` hands this one back and keeps the other four, which is
# the arm that attributes a difference to this device rather than to the four
# it now sits underneath.
#
# `native_scheduler_alone` is the arm this seam most needs, and it means
# something sharper here than its counterpart did for the controller. Every
# other device in the machine measures itself against this clock. Run alone,
# with the emulator's own sound, video and interrupt controller reading a
# native scheduler's block, a difference of a single master clock anywhere
# would show up as a different frame -- there is nowhere for it to hide.
#
# `emulated_all` hands back all five, which is what makes a matching hash a
# statement about the machine rather than about the models.
set(
    arm_names
    native
    emulated_scheduler
    native_scheduler_alone
    emulated_all
)
set(arm_flags_native "--report-scheduler-coverage")
set(arm_flags_emulated_scheduler "--emulated-scheduler")
set(
    arm_flags_native_scheduler_alone
    "--emulated-psg;--emulated-vdc;--emulated-vce;--emulated-irq"
)
set(
    arm_flags_emulated_all
    "--emulated-scheduler"
    "--emulated-psg;--emulated-vdc;--emulated-vce;--emulated-irq"
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
            "The ${arm} scheduler recorder failed (${recorder_result}).\n"
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
            "The ${arm} scheduler recording did not complete 3600 frames.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    file(SHA256 "${arm_output}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_census_sha256)
        message(
            FATAL_ERROR
            "The ${arm} scheduler census changed.\n"
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
if(NOT native_output MATCHES "Clock and event scheduler paths:")
    message(
        FATAL_ERROR
        "The native arm reported no clock and event scheduler coverage, which "
        "means the emulator answered for the device and the matching hash "
        "proves nothing.\n${native_output}"
    )
endif()

# A route's worth of advancing. This is the busiest counter in the port by two
# orders of magnitude -- everything the machine does to time arrives at
# `advance` -- so a hundred million is not a threshold that could be passed by
# a device that was installed but rarely reached.
string(
    REGEX MATCH
    "  advances: ([0-9]+) in cycles \\(([0-9]+) slow, ([0-9]+) fast\\)"
    coverage_advances
    "${native_output}"
)
if(CMAKE_MATCH_1 LESS 100000000)
    message(
        FATAL_ERROR
        "The native scheduler advanced only ${CMAKE_MATCH_1} times, which is "
        "not a route's worth of instructions.\n${native_output}"
    )
endif()

# And both rates, which is the one thing about this device the route could
# plausibly not reach. `speed` is written by two opcodes the game runs a
# handful of times in an hour; a scheduler that priced every cycle at the fast
# rate would be right almost always and wrong exactly where it mattered.
if(CMAKE_MATCH_2 LESS 1 OR CMAKE_MATCH_3 LESS 1)
    message(
        FATAL_ERROR
        "The native scheduler advanced ${CMAKE_MATCH_2} cycles at the slow "
        "rate and ${CMAKE_MATCH_3} at the fast one; a route that reaches only "
        "one of the two says nothing about the other.\n${native_output}"
    )
endif()

# The horizon, and which of the two devices behind it won. Both have to win
# somewhere: a scheduler that always stopped for the same device would be
# indistinguishable from one that only knew about that device.
string(
    REGEX MATCH
    "  recomputed ([0-9]+) times: timer nearer ([0-9]+), user nearer ([0-9]+)"
    coverage_horizon
    "${native_output}"
)
if(CMAKE_MATCH_2 LESS 1 OR CMAKE_MATCH_3 LESS 1)
    message(
        FATAL_ERROR
        "The near horizon went to the timer ${CMAKE_MATCH_2} times and to the "
        "user event ${CMAKE_MATCH_3}; both devices have to win it somewhere "
        "for the comparison to mean anything.\n${native_output}"
    )
endif()

# What the route does not reach, which for this device is a short list and an
# instructive one.
#
# `advance_master` is never called: the machine's only master-clock advances
# come from the WAI opcode, which this core does not emulate for the PC Engine,
# and from `StealMasterCycles`, which nothing in a census route calls. Nor does
# the overclock divider ever swallow an advance, because the host runs at the
# machine's own rate of one and the remainder is then always zero. Both paths
# are enumerated against the emulator's own scheduler by `dc_scheduler_device`,
# where a long randomised drive runs the rate up and the advances down until
# the divider does swallow them.
#
# This census cannot fail for a path it never takes, and printing the counts
# keeps that division of labour visible rather than implied.
string(
    REGEX MATCH
    "Clock and event scheduler paths:[^\n]*\n(  [^\n]*\n)+"
    coverage_block
    "${native_output}"
)
message(
    STATUS
    "What the route asked of the clock and event scheduler:\n"
    "${coverage_block}"
)

message(
    STATUS
    "Native scheduler device model verified: four arms reproduce "
    "${expected_census_sha256}"
)
