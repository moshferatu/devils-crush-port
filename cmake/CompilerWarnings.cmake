function(dc_enable_strict_warnings target)
    if(MSVC)
        target_compile_options(
            "${target}"
            PRIVATE
                /W4
                /permissive-
                /Zc:__cplusplus
                /utf-8
        )
    else()
        target_compile_options(
            "${target}"
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
        )
    endif()
endfunction()

