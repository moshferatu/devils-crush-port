include(FetchContent)

function(dc_provide_sdl3)
    if(TARGET SDL3::SDL3)
        return()
    endif()

    if(DC_FETCH_SDL)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
            SDL3
            URL
                https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz
            URL_HASH
                SHA256=f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            EXCLUDE_FROM_ALL
        )

        FetchContent_MakeAvailable(SDL3)

        # SDL's large static target compiles many translation units in
        # parallel. Serialize PDB writes under MSVC to avoid C1041/C1090.
        if(MSVC AND TARGET SDL3-static)
            target_compile_options(SDL3-static PRIVATE /FS)
        endif()
    else()
        find_package(SDL3 3.4 CONFIG REQUIRED)
    endif()
endfunction()

function(dc_provide_sdl3_image)
    if(TARGET SDL3_image::SDL3_image)
        return()
    endif()
    if(NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR "SDL3 must be provided before SDL3_image.")
    endif()

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_STRICT ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_BACKEND_WIC OFF CACHE BOOL "" FORCE)

    set(SDLIMAGE_ANI OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_BMP OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_GIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JPG ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_LBM OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_PCX OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_PNG ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_PNM OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_QOI OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_SVG OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TGA OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_WEBP ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_XCF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_XPM OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_XV OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_PNG_LIBPNG OFF CACHE BOOL "" FORCE)

    set(SDLIMAGE_JPG_SAVE OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_PNG_SAVE OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_WEBP_SAVE OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL3_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG bec9134a26c7d0f31b36d6083c25296e04cabff5
        GIT_SUBMODULES external/libwebp
        GIT_SUBMODULES_RECURSE TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(SDL3_image)

    if(MSVC)
        foreach(
            image_target
            IN ITEMS
                SDL3_image-static
                webp
                webpdemux
                libwebpmux
                sharpyuv
        )
            if(TARGET "${image_target}")
                target_compile_options("${image_target}" PRIVATE /FS)
            endif()
        endforeach()
    endif()
endfunction()
