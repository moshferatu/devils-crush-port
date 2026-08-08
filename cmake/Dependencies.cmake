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

