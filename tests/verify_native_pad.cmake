# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported native pad configuration: ${CONFIG}")
endif()

# The PC Engine's controller port, as a native device model rather than an
# emulated one. Two strobe lines, a multitap counter that decides which of five
# devices answers the next read, and the devices themselves.
#
# This is the eighth seam and the only one so far that is an *ownership*
# change: the strobe protocol was already modelled natively inside the
# cartridge's own pad reader at `00:E195`, and what moves here is where the
# input comes from. With the model installed the host's `InputFrame` reaches
# the machine directly and the emulator's input path -- its frontend button
# buffer, its five `PCE_Input_Device` objects -- is never consulted.
#
# **That is why this test is stronger than a matching census.** For the sound
# and video models a silent fallback would have produced the same census as the
# model, so the test had to prove the model was the default arm. Here the host
# goes further: while a pad model is installed the libretro input callback
# answers zero for every query, so the emulator's own path is not merely
# bypassed but starved. A run that had fallen back to it would be a run with
# nothing pressed, and the census route presses RUN to start a game, I to
# launch a ball, and a flipper every thirty frames for two thousand of them.
#
# Measured rather than argued: with the model installed and the callback
# starved the route still reproduces the golden census, and the model reports
# 540 of its 35,991 reads answering with a button held. Feeding the model
# nothing instead -- the same binary with one line changed -- produces
# 24b1614f..., a different route entirely, which is what makes the match above
# evidence rather than a coincidence.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

set(emulated_output "${OUTPUT}.emulated.csv")

foreach(arm IN ITEMS native emulated)
    if(arm STREQUAL "native")
        set(arm_output "${OUTPUT}")
        set(arm_flags "--report-pad-coverage")
    else()
        set(arm_output "${emulated_output}")
        set(arm_flags "--emulated-pad")
    endif()

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
            "The ${arm} pad recorder failed (${recorder_result}).\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    # The coverage below is the native arm's, and the loop would otherwise
    # leave the emulated arm's output in the variable it reads.
    if(arm STREQUAL "native")
        set(native_output "${recorder_output}")
        set(native_error "${recorder_error}")
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
            "The ${arm} pad recording did not complete 3600 frames.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    file(SHA256 "${arm_output}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_census_sha256)
        message(
            FATAL_ERROR
            "The ${arm} pad census changed.\n"
            "  expected ${expected_census_sha256}\n"
            "  actual   ${actual_sha256}"
        )
    endif()
endforeach()

# A census that matched with nothing pressed would match for the wrong reason,
# so the native arm has to say that input arrived. The count is not pinned --
# it is a property of the route rather than of the model -- but zero is a
# failure, because zero is what a starved port reports.
string(
    REGEX MATCH
    "input: ([0-9]+) reads answered with a button held"
    coverage_line
    "${native_output}"
)
if(NOT coverage_line)
    message(
        FATAL_ERROR
        "The native pad arm did not report its input coverage.\n"
        "${native_output}${native_error}"
    )
endif()
set(reads_with_a_button_down "${CMAKE_MATCH_1}")
if(reads_with_a_button_down EQUAL 0)
    message(
        FATAL_ERROR
        "The native pad answered every read with nothing held, so the census "
        "above matched without the host's input reaching the machine."
    )
endif()

# The port has paths this route cannot reach, and saying which is the point of
# the coverage counters. A six-button read needs a pad that advertises itself
# as one, which this host never sends; a nonzero count would mean the host had
# started sending a bit it does not model.
string(
    REGEX MATCH
    "([0-9]+) of a six-button pad"
    six_button_line
    "${native_output}"
)
if(NOT six_button_line OR NOT CMAKE_MATCH_1 EQUAL 0)
    message(
        FATAL_ERROR
        "The native pad reached the six-button read path, which this host "
        "has no way to ask for.\n"
        "${native_output}"
    )
endif()

# A port carrying a device the model does not answer for has been answering as
# an empty port, which is a fault rather than a variation.
string(
    REGEX MATCH
    "port\\(s\\) hold a device this model does not answer for"
    unmodelled_line
    "${native_output}"
)
if(unmodelled_line)
    message(
        FATAL_ERROR
        "The native pad was given a device it does not model.\n"
        "${native_output}"
    )
endif()

message(
    STATUS
    "Native controller port verified: both arms reproduce "
    "${expected_census_sha256}, and the native arm carried "
    "${reads_with_a_button_down} reads with a button held"
)
