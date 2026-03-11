# cmake/esp-idf.cmake
# ESP-IDF specific build configuration for microWAV

if(__wav_esp_idf_defined)
    return()
endif()
set(__wav_esp_idf_defined TRUE)

function(wav_configure_esp_idf COMPONENT_LIB COMPONENT_DIR)
    # Set optimization and warning flags
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wall
        -Wextra
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wimplicit-fallthrough
    )

    # C++ standard
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_11)

    message(STATUS "microWAV: Building for ESP-IDF target ${IDF_TARGET}")
endfunction()
