# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported native PSG configuration: ${CONFIG}")
endif()

# The PC Engine's programmable sound generator, as a native device model
# rather than an emulated one. Six channels of 32-entry five-bit waveform,
# direct digital audio, an LFO that modulates channel 0 from channel 1, and an
# 18-bit LFSR noise source on channels 4 and 5, clocked at the master clock
# divided by three and driving two band-limited delta buffers at one position
# per twelve master clocks.
#
# This is the first piece of phase 2 -- device state -- and it is a different
# kind of change from a routine transcript. Nothing about the CPU moves. The
# model accepts the same ten registers at $0800-$0809 at the same clock and
# produces the same output, so the acceptance test is that the census does not
# move at all: same work RAM, same framebuffer, and same audio, frame for
# frame, for 3,600 frames.
#
# Both arms are recorded here rather than comparing against a stored hash
# alone, because the interesting failure is not "the audio changed" but "the
# model was never installed and the emulator answered for it". A run that
# silently falls back would match the stored hash perfectly. Recording the
# emulated arm as well proves only that the two agree; what proves the model
# is doing the work is that it is the default arm and that removing it is an
# explicit flag.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)

set(emulated_output "${OUTPUT}.emulated.csv")

# Both arms hand the whole video side -- controller and colour encoder --
# back to the emulator, so this stays the test it was written to be: one
# device model, changed on its own, against the machine it replaced. The
# combinations where several models are installed together are the native
# arms of `dc_rom_native_vdc` and `dc_rom_native_vce`.
foreach(arm IN ITEMS native emulated)
    if(arm STREQUAL "native")
        set(arm_output "${OUTPUT}")
        set(arm_flags "--emulated-vdc;--emulated-vce")
    else()
        set(arm_output "${emulated_output}")
        set(arm_flags "--emulated-psg;--emulated-vdc;--emulated-vce")
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
            "The ${arm} PSG recorder failed (${recorder_result}).\n"
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
            "The ${arm} PSG recording did not complete 3600 frames.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    file(SHA256 "${arm_output}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_census_sha256)
        message(
            FATAL_ERROR
            "The ${arm} PSG census changed.\n"
            "  expected ${expected_census_sha256}\n"
            "  actual   ${actual_sha256}"
        )
    endif()
endforeach()

message(
    STATUS
    "Native PSG device model verified: both arms reproduce "
    "${expected_census_sha256}"
)
