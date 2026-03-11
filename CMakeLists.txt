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

    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/host.cmake)

    # Get sources
    wav_get_sources(${CMAKE_CURRENT_SOURCE_DIR})

    # Create the library
    add_library(micro_wav STATIC ${WAV_SOURCES})

    # Host build options
    option(ENABLE_WERROR "Treat warnings as errors" OFF)

    # Apply host configuration
    wav_configure_host(micro_wav ${CMAKE_CURRENT_SOURCE_DIR})

endif()
