# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported native VDC configuration: ${CONFIG}")
endif()

# The PC Engine's video display controller, as a native device model rather
# than an emulated one. Sixty-four kilowords of video memory behind a
# character cache, a background plane assembled from a tile map, sixty-four
# sprites of which sixteen may appear on a line, two block-transfer engines,
# six interrupt sources, and the raster phase machine that decides when any of
# it happens.
#
# The colour encoder is the emulator's in every arm here, and is handed back
# explicitly rather than by default now that a native one exists. It keeps the
# dot-clock divider, the two blanking counters, the palette and the
# framebuffer, and it drives the model with the same interleaving of clocks
# and blanking edges it drives its own chip with. What changed is which chip
# answers. The arm where both are native belongs to `dc_rom_native_vce`.
#
# The acceptance is that the census does not move at all -- and the value it
# must not move from is the one the sound slice pinned, because a video device
# that draws the same picture changes nothing a census can see. Same work RAM,
# same framebuffer, same audio, frame for frame, for 3,600 frames.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

# Three arms rather than two. The first is how the host actually runs, with
# both device models installed. The second isolates the video device by
# handing it back. The third hands back both, and is the one that matters
# most: without it the pinned hash would only prove that the two models agree
# with each other, not that they still agree with the machine they replaced.
set(arm_names native emulated_vdc emulated_both)
set(arm_flags_native "--report-vdc-coverage;--emulated-vce")
set(arm_flags_emulated_vdc "--emulated-vdc;--emulated-vce")
set(arm_flags_emulated_both "--emulated-vdc;--emulated-psg;--emulated-vce")

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
            "The ${arm} VDC recorder failed (${recorder_result}).\n"
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
            "The ${arm} VDC recording did not complete 3600 frames.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    file(SHA256 "${arm_output}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_census_sha256)
        message(
            FATAL_ERROR
            "The ${arm} VDC census changed.\n"
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
# native arm therefore also asks the model what it did, and the counts have to
# be a model's rather than an absence.
if(NOT native_output MATCHES "Video display controller paths:")
    message(
        FATAL_ERROR
        "The native arm reported no video display controller coverage, which "
        "means the emulator answered for the device and the matching hash "
        "proves nothing.\n${native_output}"
    )
endif()

string(
    REGEX MATCH
    "  fields ([0-9]+), scanlines ([0-9]+)"
    coverage_fields
    "${native_output}"
)
if(CMAKE_MATCH_1 LESS 3000 OR CMAKE_MATCH_2 LESS 900000)
    message(
        FATAL_ERROR
        "The native video device drew ${CMAKE_MATCH_1} fields and "
        "${CMAKE_MATCH_2} scanlines, which is not a 3,600-frame route.\n"
        "${native_output}"
    )
endif()

# What the route does not reach is the more useful half of the measurement,
# and on this route that is most of the chip. Devil's Crush never puts a
# seventeenth sprite on a line, never collides sprite zero, never asks for a
# 16-colour sprite, and never runs the video-memory block transfer engine at
# all -- it moves its sprite table and nothing else. It also never exhausts
# the encoder's per-frame budget of stolen cycles, so the path where a stalled
# port access has to finish its own transfer is never taken here.
#
# Those are exactly the paths `dc_vdc_device` drives with synthetic stimulus
# against the emulator's own device, because this census cannot fail for a
# path it never takes. Printing them keeps the division of labour visible
# rather than implied.
string(
    REGEX MATCH
    "Video display controller paths:[^\n]*\n(  [^\n]*\n)+"
    coverage_block
    "${native_output}"
)
message(STATUS "What the route asked of the video device:\n${coverage_block}")

message(
    STATUS
    "Native VDC device model verified: three arms reproduce "
    "${expected_census_sha256}"
)
