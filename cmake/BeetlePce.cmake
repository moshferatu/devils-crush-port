include(FetchContent)

set(
    DC_BEETLE_PCE_REVISION
    "ae99235c2139c176c1a8d0fde2957bf701d3cab0"
)

function(dc_provide_beetle_pce)
    if(TARGET DevilsCrush::BeetlePce)
        return()
    endif()

    if(NOT MSVC)
        message(
            FATAL_ERROR
            "The bundled Beetle PCE integration currently supports only "
            "Microsoft Visual C++ on Windows."
        )
    endif()

    FetchContent_Declare(
        dc_beetle_pce_upstream
        GIT_REPOSITORY https://github.com/libretro/beetle-pce-libretro.git
        GIT_TAG "${DC_BEETLE_PCE_REVISION}"
        GIT_SHALLOW FALSE
        GIT_PROGRESS TRUE
        GIT_SUBMODULES ""
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(dc_beetle_pce_upstream)

    set(beetle_root "${dc_beetle_pce_upstream_SOURCE_DIR}")
    set(
        beetle_fork_root
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../third_party/beetle-pce-fork"
    )
    set(
        beetle_patches
        "${beetle_fork_root}/patches/0001-huc6280-execution-hook.patch"
        "${beetle_fork_root}/patches/0002-huc6280-call-observation.patch"
        "${beetle_fork_root}/patches/0003-huc6280-native-replacement.patch"
        "${beetle_fork_root}/patches/0004-vdc-video-snapshot.patch"
        "${beetle_fork_root}/patches/0005-vdc-sprite-snapshot.patch"
        "${beetle_fork_root}/patches/0006-huc6280-interruptible-replacement.patch"
        "${beetle_fork_root}/patches/0007-huc6280-nested-replacement.patch"
        "${beetle_fork_root}/patches/0008-psg-device-model.patch"
        "${beetle_fork_root}/patches/0009-vdc-device-model.patch"
        "${beetle_fork_root}/patches/0010-vce-reference-observation.patch"
        "${beetle_fork_root}/patches/0011-vce-device-model.patch"
        "${beetle_fork_root}/patches/0012-huc6280-irq-reference-observation.patch"
        "${beetle_fork_root}/patches/0013-huc6280-irq-device-model.patch"
        "${beetle_fork_root}/patches/0014-huc6280-scheduler-reference-observation.patch"
        "${beetle_fork_root}/patches/0015-huc6280-scheduler-device-model.patch"
        "${beetle_fork_root}/patches/0016-huc6280-instruction-boundary-reference-observation.patch"
        "${beetle_fork_root}/patches/0017-huc6280-instruction-boundary-device-model.patch"
        "${beetle_fork_root}/patches/0018-huc6280-idle-fast-forward.patch"
        "${beetle_fork_root}/patches/0019-joypad-device-model.patch"
    )

    find_package(Git REQUIRED)
    foreach(beetle_patch IN LISTS beetle_patches)
        execute_process(
            COMMAND
                "${GIT_EXECUTABLE}" -C "${beetle_root}"
                apply --check "${beetle_patch}"
            RESULT_VARIABLE beetle_patch_check
            OUTPUT_VARIABLE beetle_patch_check_output
            ERROR_VARIABLE beetle_patch_check_error
        )
        if(beetle_patch_check EQUAL 0)
            execute_process(
                COMMAND
                    "${GIT_EXECUTABLE}" -C "${beetle_root}"
                    apply "${beetle_patch}"
                RESULT_VARIABLE beetle_patch_result
                OUTPUT_VARIABLE beetle_patch_output
                ERROR_VARIABLE beetle_patch_error
            )
            if(NOT beetle_patch_result EQUAL 0)
                message(
                    FATAL_ERROR
                    "Could not apply Beetle PCE downstream patch "
                    "${beetle_patch}.\n"
                    "${beetle_patch_output}${beetle_patch_error}"
                )
            endif()
        else()
            file(
                READ
                "${beetle_root}/mednafen/pce/huc6280.cpp"
                beetle_cpu_source
            )
            file(
                READ
                "${beetle_root}/mednafen/pce/huc6280.h"
                beetle_cpu_header
            )
            file(
                READ
                "${beetle_root}/mednafen/pce/vce.h"
                beetle_video_header
            )
            file(
                READ
                "${beetle_root}/mednafen/pce/vce.cpp"
                beetle_video_source
            )
            file(
                READ
                "${beetle_root}/mednafen/hw_video/huc6270/vdc.h"
                beetle_vdc_header
            )
            file(
                READ
                "${beetle_root}/mednafen/hw_sound/pce_psg/pce_psg.h"
                beetle_psg_header
            )
            file(
                READ
                "${beetle_root}/mednafen/pce/pce.cpp"
                beetle_machine_source
            )
            file(
                READ
                "${beetle_root}/mednafen/pce/input.cpp"
                beetle_input_source
            )
            file(
                READ
                "${beetle_root}/mednafen/pce/input/gamepad.cpp"
                beetle_gamepad_source
            )
            get_filename_component(
                beetle_patch_name
                "${beetle_patch}"
                NAME
            )
            if(beetle_patch_name MATCHES "^0001-")
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "dc_beetle_cpu_state hook_state"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "dc_beetle_active_instruction_hook_user_data"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0002-")
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "HookInstructionPC = (uint16)PC"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_header}"
                    "dc_beetle_active_memory_write_hook_user_data"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0003-")
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "dc_beetle_replacement_result replacement"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "dc_beetle_active_replacement_hook_user_data"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0004-")
                string(
                    FIND
                    "${beetle_video_header}"
                    "DebugPeekVRAM"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_video_header}"
                    "DebugGetVDCRegister"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0005-")
                string(
                    FIND
                    "${beetle_video_header}"
                    "DebugPeekSAT"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_video_header}"
                    "return vdc[0].PeekSAT"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0006-")
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "ApplyReplacementCheckpoint"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_header}"
                    "ReplacementCheckpoints"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0007-")
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "PushReplacementFrame"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_header}"
                    "ReplacementSuspended"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0008-")
                string(
                    FIND
                    "${beetle_psg_header}"
                    "DcExportState"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_machine_source}"
                    "dc_beetle_set_psg_device"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0009-")
                string(
                    FIND
                    "${beetle_vdc_header}"
                    "DcExportState"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_video_source}"
                    "dc_beetle_active_vdc_model"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0010-")
                string(
                    FIND
                    "${beetle_video_header}"
                    "DcObservation"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_video_source}"
                    "VCE::DcObserve"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0011-")
                string(
                    FIND
                    "${beetle_video_header}"
                    "DcOwned"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_video_source}"
                    "dc_beetle_vce_vdc_reset"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0012-")
                string(
                    FIND
                    "${beetle_cpu_header}"
                    "DcIrqObservation"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "HuC6280::DcObserveIrq"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0013-")
                string(
                    FIND
                    "${beetle_cpu_header}"
                    "DcIrqPublish"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "dc_beetle_active_irq_model"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0014-")
                string(
                    FIND
                    "${beetle_cpu_header}"
                    "DcSchedulerObservation"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "HuC6280::DcObserveScheduler"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0015-")
                string(
                    FIND
                    "${beetle_cpu_header}"
                    "dc_beetle_active_scheduler_registers"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "DcSpeedChange"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0016-")
                string(
                    FIND
                    "${beetle_cpu_header}"
                    "DcSelectInterrupt"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "HuC6280::DcObserveBoundary"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0017-")
                string(
                    FIND
                    "${beetle_cpu_header}"
                    "dc_beetle_active_boundary_model"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "DcResetInterruptSample"
                    beetle_patch_marker_two
                )
            elseif(beetle_patch_name MATCHES "^0018-")
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "dc_beetle_active_idle_model"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_cpu_source}"
                    "iteration_cycles"
                    beetle_patch_marker_two
                )
            else()
                string(
                    FIND
                    "${beetle_input_source}"
                    "dc_beetle_active_pad_model"
                    beetle_patch_marker_one
                )
                string(
                    FIND
                    "${beetle_gamepad_source}"
                    "DcExportState"
                    beetle_patch_marker_two
                )
            endif()
            if(beetle_patch_marker_one EQUAL -1
               OR beetle_patch_marker_two EQUAL -1)
                message(
                    FATAL_ERROR
                    "The Beetle PCE source is neither patchable nor already "
                    "patched by ${beetle_patch}.\n"
                    "${beetle_patch_check_output}"
                    "${beetle_patch_check_error}"
                )
            endif()
        endif()
    endforeach()

    set(
        beetle_cxx_sources
        "${beetle_fork_root}/src/dc_beetle_execution_hook.cpp"
        "${beetle_fork_root}/src/dc_beetle_reference_boundary.cpp"
        "${beetle_fork_root}/src/dc_beetle_reference_irq.cpp"
        "${beetle_fork_root}/src/dc_beetle_reference_psg.cpp"
        "${beetle_fork_root}/src/dc_beetle_reference_scheduler.cpp"
        "${beetle_fork_root}/src/dc_beetle_reference_vce.cpp"
        "${beetle_fork_root}/src/dc_beetle_reference_vdc.cpp"
        "${beetle_root}/libretro.cpp"
        "${beetle_root}/mednafen/pce/huc6280.cpp"
        "${beetle_root}/mednafen/pce/huc.cpp"
        "${beetle_root}/mednafen/pce/input.cpp"
        "${beetle_root}/mednafen/pce/mcgenjin.cpp"
        "${beetle_root}/mednafen/pce/pce.cpp"
        "${beetle_root}/mednafen/pce/pcecd.cpp"
        "${beetle_root}/mednafen/pce/tsushin.cpp"
        "${beetle_root}/mednafen/pce/vce.cpp"
        "${beetle_root}/mednafen/pce/input/gamepad.cpp"
        "${beetle_root}/mednafen/pce/input/mouse.cpp"
        "${beetle_root}/mednafen/pce/input/tsushinkb.cpp"
        "${beetle_root}/mednafen/hw_misc/arcade_card/arcade_card.cpp"
        "${beetle_root}/mednafen/hw_sound/pce_psg/pce_psg.cpp"
        "${beetle_root}/mednafen/hw_video/huc6270/vdc.cpp"
        "${beetle_root}/mednafen/cdrom/CDAccess.cpp"
        "${beetle_root}/mednafen/cdrom/CDAccess_Image.cpp"
        "${beetle_root}/mednafen/cdrom/CDAccess_CCD.cpp"
        "${beetle_root}/mednafen/cdrom/CDAFReader.cpp"
        "${beetle_root}/mednafen/cdrom/CDAFReader_Vorbis.cpp"
        "${beetle_root}/mednafen/cdrom/cdromif.cpp"
        "${beetle_root}/mednafen/cdrom/CDUtility.cpp"
        "${beetle_root}/mednafen/cdrom/lec.cpp"
        "${beetle_root}/mednafen/cdrom/galois.cpp"
        "${beetle_root}/mednafen/cdrom/recover-raw.cpp"
        "${beetle_root}/mednafen/cdrom/l-ec.cpp"
        "${beetle_root}/mednafen/cdrom/edc_crc32.cpp"
        "${beetle_root}/mednafen/cdrom/scsicd.cpp"
        "${beetle_root}/mednafen/cdrom/CDAccess_CHD.cpp"
        "${beetle_root}/mednafen/settings.cpp"
        "${beetle_root}/mednafen/general.cpp"
        "${beetle_root}/mednafen/FileStream.cpp"
        "${beetle_root}/mednafen/MemoryStream.cpp"
        "${beetle_root}/mednafen/Stream.cpp"
        "${beetle_root}/mednafen/state.cpp"
        "${beetle_root}/mednafen/mempatcher.cpp"
        "${beetle_root}/mednafen/sound/okiadpcm.cpp"
        "${beetle_root}/mednafen/sound/OwlResampler.cpp"
    )

    set(
        beetle_c_sources
        "${beetle_root}/mednafen/sound/Blip_Buffer.c"
        "${beetle_root}/mednafen/file.c"
        "${beetle_root}/mednafen/mednafen-endian.c"
        "${beetle_root}/mednafen/cputest/cputest.c"
        "${beetle_root}/libretro-common/streams/file_stream.c"
        "${beetle_root}/libretro-common/streams/file_stream_transforms.c"
        "${beetle_root}/libretro-common/file/file_path.c"
        "${beetle_root}/libretro-common/file/retro_dirent.c"
        "${beetle_root}/libretro-common/lists/string_list.c"
        "${beetle_root}/libretro-common/lists/dir_list.c"
        "${beetle_root}/libretro-common/compat/compat_strl.c"
        "${beetle_root}/libretro-common/compat/compat_snprintf.c"
        "${beetle_root}/libretro-common/compat/compat_posix_string.c"
        "${beetle_root}/libretro-common/compat/compat_strcasestr.c"
        "${beetle_root}/libretro-common/compat/fopen_utf8.c"
        "${beetle_root}/libretro-common/encodings/encoding_utf.c"
        "${beetle_root}/libretro-common/encodings/encoding_crc32.c"
        "${beetle_root}/libretro-common/vfs/vfs_implementation.c"
        "${beetle_root}/libretro-common/memmap/memalign.c"
        "${beetle_root}/libretro-common/string/stdstring.c"
        "${beetle_root}/libretro-common/time/rtime.c"
        "${beetle_root}/deps/lzma-19.00/src/Alloc.c"
        "${beetle_root}/deps/lzma-19.00/src/Bra86.c"
        "${beetle_root}/deps/lzma-19.00/src/BraIA64.c"
        "${beetle_root}/deps/lzma-19.00/src/CpuArch.c"
        "${beetle_root}/deps/lzma-19.00/src/Delta.c"
        "${beetle_root}/deps/lzma-19.00/src/LzFind.c"
        "${beetle_root}/deps/lzma-19.00/src/Lzma86Dec.c"
        "${beetle_root}/deps/lzma-19.00/src/LzmaDec.c"
        "${beetle_root}/deps/lzma-19.00/src/LzmaEnc.c"
        "${beetle_root}/deps/libchdr/src/libchdr_bitstream.c"
        "${beetle_root}/deps/libchdr/src/libchdr_cdrom.c"
        "${beetle_root}/deps/libchdr/src/libchdr_chd.c"
        "${beetle_root}/deps/libchdr/src/libchdr_flac.c"
        "${beetle_root}/deps/libchdr/src/libchdr_huffman.c"
        "${beetle_root}/deps/zstd/lib/common/entropy_common.c"
        "${beetle_root}/deps/zstd/lib/common/error_private.c"
        "${beetle_root}/deps/zstd/lib/common/fse_decompress.c"
        "${beetle_root}/deps/zstd/lib/common/zstd_common.c"
        "${beetle_root}/deps/zstd/lib/common/xxhash.c"
        "${beetle_root}/deps/zstd/lib/decompress/huf_decompress.c"
        "${beetle_root}/deps/zstd/lib/decompress/zstd_ddict.c"
        "${beetle_root}/deps/zstd/lib/decompress/zstd_decompress.c"
        "${beetle_root}/deps/zstd/lib/decompress/zstd_decompress_block.c"
        "${beetle_root}/deps/zlib-1.2.11/adler32.c"
        "${beetle_root}/deps/zlib-1.2.11/crc32.c"
        "${beetle_root}/deps/zlib-1.2.11/inffast.c"
        "${beetle_root}/deps/zlib-1.2.11/inflate.c"
        "${beetle_root}/deps/zlib-1.2.11/inftrees.c"
        "${beetle_root}/deps/zlib-1.2.11/zutil.c"
        "${beetle_root}/mednafen/tremor/bitwise.c"
        "${beetle_root}/mednafen/tremor/block.c"
        "${beetle_root}/mednafen/tremor/codebook.c"
        "${beetle_root}/mednafen/tremor/floor0.c"
        "${beetle_root}/mednafen/tremor/floor1.c"
        "${beetle_root}/mednafen/tremor/framing.c"
        "${beetle_root}/mednafen/tremor/info.c"
        "${beetle_root}/mednafen/tremor/mapping0.c"
        "${beetle_root}/mednafen/tremor/mdct.c"
        "${beetle_root}/mednafen/tremor/registry.c"
        "${beetle_root}/mednafen/tremor/res012.c"
        "${beetle_root}/mednafen/tremor/sharedbook.c"
        "${beetle_root}/mednafen/tremor/synthesis.c"
        "${beetle_root}/mednafen/tremor/vorbisfile.c"
        "${beetle_root}/mednafen/tremor/window.c"
    )

    add_library(
        dc_beetle_pce
        STATIC
        ${beetle_cxx_sources}
        ${beetle_c_sources}
    )
    add_library(DevilsCrush::BeetlePce ALIAS dc_beetle_pce)

    target_include_directories(
        dc_beetle_pce
        SYSTEM
        PUBLIC
            "${beetle_root}/libretro-common/include"
            "${beetle_fork_root}/include"
        PRIVATE
            "${beetle_root}"
            "${beetle_root}/mednafen"
            "${beetle_root}/mednafen/include"
            "${beetle_root}/mednafen/hw_sound"
            "${beetle_root}/mednafen/hw_cpu"
            "${beetle_root}/mednafen/hw_misc"
            "${beetle_root}/deps/lzma-19.00/include"
            "${beetle_root}/deps/libchdr/include"
            "${beetle_root}/deps/zstd/lib"
            "${beetle_root}/deps/zlib-1.2.11"
    )

    target_compile_features(dc_beetle_pce PRIVATE cxx_std_20)
    target_compile_definitions(
        dc_beetle_pce
        PUBLIC
            RETRO_API=
        PRIVATE
            WINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP
            PACKAGE="mednafen"
            MEDNAFEN_VERSION_NUMERIC=931
            WANT_PCE_EMU
            WANT_STEREO_SOUND
            STDC_HEADERS
            __STDC_LIMIT_MACROS
            __LIBRETRO__
            _LOW_ACCURACY_
            INLINE=_inline
            WANT_16BPP
            FRONTEND_SUPPORTS_RGB565
            NEED_CD
            HAVE_CHD
            _7ZIP_ST
            ZSTD_DISABLE_ASM
            NEED_TREMOR
            GIT_VERSION=" ae99235"
    )

    if(MSVC)
        target_compile_options(dc_beetle_pce PRIVATE /W0 /FS)
    endif()

    set_target_properties(
        dc_beetle_pce
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
    )
endfunction()
