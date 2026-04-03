# CMakeLists.txt for microWAV
# Supports both ESP-IDF component builds and standalone host builds
#
# Structure:
#   cmake/sources.cmake   - Source file definitions
#   cmake/esp-idf.cmake   - ESP-IDF specific configuration
#   cmake/host.cmake      - Host platform configuration

cmake_minimum_required(VERSION 3.16)

# ==============================================================================
# Build type detection
# ==============================================================================

if(DEFINED IDF_TARGET)
    set(ESP_IDF_BUILD TRUE)
else()
    set(ESP_IDF_BUILD FALSE)
endif()

# Set source directory variable for consistent use
if(ESP_IDF_BUILD)
    set(WAV_COMPONENT_DIR ${COMPONENT_DIR})
else()
    set(WAV_COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ==============================================================================
# Include CMake modules
# ==============================================================================

include(${WAV_COMPONENT_DIR}/cmake/sources.cmake)

# ==============================================================================
# ESP-IDF Build
# ==============================================================================

if(ESP_IDF_BUILD)
    # Collect sources
    wav_get_sources(${WAV_COMPONENT_DIR})

    include(${WAV_COMPONENT_DIR}/cmake/esp-idf.cmake)

    # Register the component
    idf_component_register(
        SRCS ${WAV_SOURCES}
        INCLUDE_DIRS "include"
        PRIV_INCLUDE_DIRS "src"
    )

    # Apply ESP-IDF configuration
    wav_configure_esp_idf(${COMPONENT_LIB} ${COMPONENT_DIR})

# ==============================================================================
# Host Build
# ==============================================================================

else()
    project(micro_wav VERSION 0.1.0 LANGUAGES CXX)

    # Host build options
    option(ENABLE_WERROR "Treat warnings as errors" OFF)
    option(ENABLE_TESTS "Build tests" OFF)
    option(BUILD_TEST_WAV_WRITER "Build WAV file writer for sox validation" OFF)

    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/host.cmake)

    # Get sources
    wav_get_sources(${CMAKE_CURRENT_SOURCE_DIR})

    # Create the library
    add_library(micro_wav STATIC ${WAV_SOURCES})

    # Apply host configuration
    wav_configure_host(micro_wav ${CMAKE_CURRENT_SOURCE_DIR})

    # Tests
    if(ENABLE_TESTS)
        enable_testing()

        add_executable(micro_wav_tests tests/test_wav_decoder.cpp)
        target_link_libraries(micro_wav_tests PRIVATE micro_wav)
        target_include_directories(micro_wav_tests PRIVATE tests)
        wav_configure_host(micro_wav_tests ${CMAKE_CURRENT_SOURCE_DIR})

        add_test(NAME wav_decoder_tests COMMAND micro_wav_tests)
    endif()

    # WAV file writer for manual sox validation (not part of CI)
    if(BUILD_TEST_WAV_WRITER)
        add_executable(write_test_wavs tests/write_test_wavs.cpp)
        target_include_directories(write_test_wavs PRIVATE tests)
    endif()

endif()
