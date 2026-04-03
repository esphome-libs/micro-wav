# cmake/host.cmake
# Host platform build configuration for microWAV

if(__wav_host_defined)
    return()
endif()
set(__wav_host_defined TRUE)

message(STATUS "microWAV: Building for host platform")

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

    # C++ standard: PUBLIC for libraries (propagates to consumers), PRIVATE for executables
    get_target_property(_target_type ${TARGET} TYPE)
    if(_target_type STREQUAL "EXECUTABLE")
        target_compile_features(${TARGET} PRIVATE cxx_std_11)
    else()
        target_compile_features(${TARGET} PUBLIC cxx_std_11)
    endif()

endfunction()
