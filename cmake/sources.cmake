# cmake/sources.cmake
# Source file definitions for microWAV

if(__wav_sources_defined)
    return()
endif()
set(__wav_sources_defined TRUE)

function(wav_get_sources SOURCE_DIR)
    set(WAV_SOURCES
        ${SOURCE_DIR}/src/wav_decoder.cpp
        PARENT_SCOPE
    )
endfunction()
