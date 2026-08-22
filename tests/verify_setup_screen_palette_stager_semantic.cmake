# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Setup-screen palette stager regression is Release-only under the native-port cadence.")
endif()

# The setup screen's block stager, and the palette shadow buffer's only
# producer outside bank 00.
#
# 08:5961 is the scheduler-owned four-tick cadence controller. It either
# decrements the indexed delay and leaves its RTS to the core, or reloads that
# delay, flips the colour-set phase, and crosses its authentic JSR into 08:5977.
# 08:5977 stages thirty-two bytes into block zero of the buffer at $27B9 and
# raises the dirty flags at $2CCC and $2CCB, which is 00:EBA5's job for the one
# block the title, play-mode, speed and data-load screens animate. Its caller
# flips bit 0 of the cadence at `$3A80,X` every four ticks, and the phase picks
# which of two colour sets is staged, so the screen flashes between them.
#
# Where it differs from 00:EBA5 is the source. This routine builds its own:
# `$04`/`$1E` into `$2F`/`$30`, then `JSR $E1FD`, which maps bank `$1E` at
# `$8000` and `$1F` at `$A000` and replaces the pair with the pointer it reads
# out of `$8004`. The phase is then shifted left five times into a sixteen-bit
# displacement and added on. So the thirty-two loads run against two ROM banks
# that a call one instruction earlier put in MPR4 and MPR5, resolved through the
# MPRs the transcript declares rather than assumed.
#
#   08:5961  cadence decrement/reload and the `JSR $5977`
#   08:5977  build the asset reference, then make the `JSR $E1FD`
#   08:5982  the displacement, the copy, the two flags, and the `RTS`
#
# The resolver is crossed, not composed: it exists to switch banks, and both of
# its halves have been native since the call-wrapper slice, so the walk pushes
# the return, charges the cycles and ends on the callee's first instruction.
# 08:5982 is the address that call comes back to, and it ends past the `RTS`
# that closes the call 08:5977 was entered on; what that costs the routine-call
# observer is proved separately by dc_rom_absorbed_return_trace_resolved.
#
# What the scope proves. The whole stack page is in range because the crossing
# pushes a return address there and lands wherever the caller left S. `$202E`-
# `$2030` is the pointer the walk builds -- the scratch high byte and both bytes
# of the source pointer, which is the arithmetic the routine exists to do. The
# other two ranges are the whole shadow buffer and all thirty-two dirty flags
# rather than block zero and its two flags, so a walk that staged the wrong
# block, or staged more than one, would show. Both artifacts were recorded from
# a run of the same route with the family emulated, so the writes match in
# value, address, order and master-cycle offset rather than merely in end state,
# and the other producers of the buffer -- the crossfade, the fade-to-black
# stepper and 00:EBA5 -- stay emulated in this run and have to interleave with
# this one exactly as they did.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "3fb9b0613a209539757ca4185cc1c264d8e49e88c7af3b71fece23db2887b634"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "08:5961@all"
        --replace-hook
        "08:5977@all"
        --replace-hook
        "08:5982@all"
        --work-ram-write-range
        "2100-21FF"
        --work-ram-write-range
        "202E-2030"
        --work-ram-write-range
        "27B9-2BB8"
        --work-ram-write-range
        "2CCB-2CEB"
        --work-ram-write-trace
        "${write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic setup-screen palette stager recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 08:5961@all entries: 670 of 670"
    "Native replacement 08:5977@all entries: 177 of 177"
    "Native replacement 08:5982@all entries: 177 of 177"
    "Work-RAM write trace complete: 5763953 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic setup-screen palette stager output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    else()
        set(artifact_path "${write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Semantic setup-screen palette stager ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic setup-screen palette cadence/stager runs all 670 cadence and "
    "177 stager calls natively, crosses both authentic JSRs, leaves the "
    "scheduler RTS to the core, and preserves the phase-selected block-zero "
    "shadow-buffer writes, dirty flags, pointer, stack page, and state census "
    "exactly."
)
