# SPDX-License-Identifier: GPL-3.0-or-later
# Replay the user's pentagram capture against the original pattern loader.
# The captured hashes describe the bug, so keep the inputs and checkpoint but
# compare corrected execution against the oracle instead of those hashes.
foreach(required IN ITEMS APP ROM CAPTURE OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
file(READ "${CAPTURE}/inputs.csv" inputs)
string(REPLACE "# format=devils-crush-interactive-input-v3"
    "# format=devils-crush-interactive-input-v1" inputs "${inputs}")
string(REPLACE "# host_mode=cabinet\n" "" inputs "${inputs}")
string(REPLACE ",work_ram_sha256" "" inputs "${inputs}")
string(REGEX REPLACE ",[0-9a-f]+\n" "\n" inputs "${inputs}")
foreach(mode IN ITEMS assembly native)
    set(dir "${OUTPUT}/${mode}")
    file(MAKE_DIRECTORY "${dir}")
    file(COPY "${CAPTURE}/initial.state" "${CAPTURE}/initial.hiscore"
        DESTINATION "${dir}")
    file(CONFIGURE OUTPUT "${dir}/inputs.csv" CONTENT "${inputs}"
        @ONLY NEWLINE_STYLE UNIX)
    if(mode STREQUAL "native")
        set(hooks --replace-hook "00:F768@all" --replace-hook "00:F778@all"
            --replace-hook "00:F781@all")
    else()
        set(hooks --registry-exclude "00:F768" --registry-exclude "00:F778"
            --registry-exclude "00:F781")
    endif()
    execute_process(COMMAND "${APP}" --replay-session "${dir}/inputs.csv"
        --cabinet --replace-registry ${hooks} --observe-hook "00:F768"
        --routine-call-trace "${dir}/calls.csv" --rom "${ROM}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(WRITE "${dir}/run.log" "${output}${error}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Face ${mode} replay failed: ${output}${error}")
    endif()
    foreach(expected IN ITEMS "Interactive session replay recording complete: 4366 frames"
            "Registry abandonments: 0 transcripts given up part-way through")
        string(FIND "${output}" "${expected}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Face ${mode} replay omitted ${expected}")
        endif()
    endforeach()
    if(mode STREQUAL "native")
        foreach(pc IN ITEMS F768 F778 F781)
            string(FIND "${output}" "Native replacement 00:${pc}@all entries: 3 of 3" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "Face loader ${pc} was not exercised three times: ${output}")
            endif()
        endforeach()
    endif()
endforeach()
foreach(artifact IN ITEMS replay.csv calls.csv calls.csv.writes.csv)
    file(SHA256 "${OUTPUT}/assembly/${artifact}" original)
    file(SHA256 "${OUTPUT}/native/${artifact}" replacement)
    if(NOT original STREQUAL replacement)
        message(FATAL_ERROR "Face ${artifact}: ${original} != ${replacement}")
    endif()
    message(STATUS "Face ${artifact}: ${original}")
endforeach()
