# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported native VCE configuration: ${CONFIG}")
endif()

# The PC Engine's colour encoder, as a native device model rather than an
# emulated one. A 512-entry palette, a dot-clock divider that turns the master
# clock into dot clocks at one of four rates, two blanking counters, the
# scanline bookkeeping -- and the schedule that interleaves all of it with the
# video display controller and writes the result into the frontend's
# framebuffer.
#
# This is the first device in this port that owns time rather than answering
# it. The emulator's processor now runs until the model says to stop, and the
# picture is drawn by the model rather than handed to it. What stays behind is
# the display's business rather than the chip's: what a nine-bit palette entry
# looks like on a screen, how a frame that mixed two dot rates is rescaled,
# and which rectangle the frontend should show.
#
# The acceptance is that the census does not move at all, and the value it
# must not move from is the one the sound slice pinned and the controller
# slice reproduced. Same work RAM, same framebuffer, same audio, frame for
# frame, for 3,600 frames -- and the framebuffer column is 3,600 separate
# hashes of the picture, so a single wrong pixel on a single frame fails this.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

# Four arms, and the fourth is what earns the other three.
#
# `native` is how the host actually runs: both video models installed, the
# encoder clocking the controller with no emulator between them. It is the
# first time in this port that two native devices talk to each other.
#
# The two middle arms take that pair apart. `emulated_vdc` keeps the native
# encoder and hands the controller back, which is the arm the whole
# `VdcDevice` seam exists for -- without it, a difference could only be
# attributed to "the video side" rather than to one chip. `emulated_vce` is
# the mirror: the native controller driven by the emulator's own encoder,
# which is exactly what `dc_rom_native_vdc` proves, repeated here because it
# is one of the four combinations this pair can be in.
#
# `emulated_all` hands back all three device models. Without it a matching
# hash would only show that the models agree with each other on this machine;
# with it, it shows they agree with the machine they replaced.
set(arm_names native emulated_vdc emulated_vce emulated_all)
set(arm_flags_native "--report-vce-coverage")
set(arm_flags_emulated_vdc "--emulated-vdc")
set(arm_flags_emulated_vce "--emulated-vce")
set(arm_flags_emulated_all "--emulated-vce;--emulated-vdc;--emulated-psg")

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
            "The ${arm} VCE recorder failed (${recorder_result}).\n"
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
            "The ${arm} VCE recording did not complete 3600 frames.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    file(SHA256 "${arm_output}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_census_sha256)
        message(
            FATAL_ERROR
            "The ${arm} VCE census changed.\n"
            "  expected ${expected_census_sha256}\n"
            "  actual   ${actual_sha256}"
        )
    endif()

    if(arm STREQUAL "native")
        set(native_output "${recorder_output}")
    endif()
endforeach()

# A matching hash proves nothing on its own if the model was never installed,
# because the emulator would have answered and produced exactly this hash. The
# native arm therefore also asks the model what it did.
if(NOT native_output MATCHES "Colour encoder paths:")
    message(
        FATAL_ERROR
        "The native arm reported no colour encoder coverage, which means the "
        "emulator answered for the device and the matching hash proves "
        "nothing.\n${native_output}"
    )
endif()

# The counts that say a whole route ran through the model rather than a
# fragment of one: a field for every recorded frame, and a picture's worth of
# pixels actually written. A model that had been installed but bypassed after
# the first frame would still print a heading.
string(
    REGEX MATCH
    "  frames ([0-9]+), partials ([0-9]+), completed ([0-9]+)"
    coverage_frames
    "${native_output}"
)
if(NOT CMAKE_MATCH_1 EQUAL 3600 OR NOT CMAKE_MATCH_3 EQUAL 3600)
    message(
        FATAL_ERROR
        "The native encoder started ${CMAKE_MATCH_1} frames and completed "
        "${CMAKE_MATCH_3}, which is not a 3,600-frame route.\n"
        "${native_output}"
    )
endif()

string(
    REGEX MATCH
    "  pixels ([0-9]+), off-screen runs ([0-9]+)"
    coverage_pixels
    "${native_output}"
)
if(CMAKE_MATCH_1 LESS 200000000)
    message(
        FATAL_ERROR
        "The native encoder wrote ${CMAKE_MATCH_1} pixels, which is not a "
        "drawn route.\n${native_output}"
    )
endif()

# What the route does not reach is again the more useful half. Devil's Crush
# runs at one dot rate and never changes it, in colour, on a 262-line field,
# without the upscaled pixel path and without the horizontal overscan. It
# never reads the palette back, never runs a compact disc unit, and -- the one
# that matters most -- never spends a frame's budget of 455 x 64 stolen
# cycles, so the refusal that is the only thing ending a stalled port access's
# wait loop is never taken here.
#
# Those are the paths `dc_vce_device` drives with synthetic stimulus against
# the emulator's own encoder inside a bare machine. This census cannot fail
# for a path it never takes; printing the counts keeps that division of labour
# visible rather than implied.
string(
    REGEX MATCH
    "Colour encoder paths:[^\n]*\n(  [^\n]*\n)+"
    coverage_block
    "${native_output}"
)
message(
    STATUS
    "What the route asked of the colour encoder:\n${coverage_block}"
)

message(
    STATUS
    "Native VCE device model verified: four arms reproduce "
    "${expected_census_sha256}"
)
