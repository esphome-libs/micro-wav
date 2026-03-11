# cmake/host.cmake
# Host platform build configuration for microWAV

if(__wav_host_defined)
    return()
endif()
set(__wav_host_defined TRUE)

function(wav_configure_host TARGET SOURCE_DIR)
    target_include_directories(${TARGET} PUBLIC
        ${SOURCE_DIR}/include
    )

    target_include_directories(${TARGET} PRIVATE
        ${SOURCE_DIR}/src
    )

    target_compile_options(${TARGET} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        $<$<BOOL:${ENABLE_WERROR}>:-Werror>
    )

    # C++ standard
    target_compile_features(${TARGET} PUBLIC cxx_std_11)

    message(STATUS "microWAV: Building for host platform")
endfunction()
