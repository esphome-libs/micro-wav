cmake_minimum_required(VERSION 3.16)
project(micro_wav_fuzz CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libFuzzer is the default. Apple clang ships without libclang_rt.fuzzer, so
# point CMAKE_CXX_COMPILER at Homebrew LLVM (e.g. /opt/homebrew/opt/llvm/bin/clang++).
# Pass -DFUZZ_USE_LIBFUZZER=OFF for the standalone harness instead.
option(FUZZ_USE_LIBFUZZER "Build as a libFuzzer target" ON)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Fuzzing requires Clang. Set CMAKE_CXX_COMPILER to clang++.")
endif()

# AppleClang satisfies the MATCHES "Clang" check above but ships without the
# libFuzzer runtime, so a libFuzzer build only fails later at link with a
# confusing error. Fail early with guidance. The standalone build
# (-DFUZZ_USE_LIBFUZZER=OFF) needs only ASan/UBSan and works under AppleClang.
if(FUZZ_USE_LIBFUZZER AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    message(FATAL_ERROR
        "AppleClang has no libFuzzer runtime. Point CMAKE_CXX_COMPILER at Homebrew "
        "LLVM (e.g. $(brew --prefix llvm)/bin/clang++), or pass "
        "-DFUZZ_USE_LIBFUZZER=OFF for the standalone harness.")
endif()

# ASan + UBSan, all checks on. The WAV decoder uses only unsigned shifts, bounded
# integer math, and clamped float->int conversion, so it is UBSan-clean and needs
# no suppressions (contrast the Vorbis fork, whose fixed-point DSP relies on
# deliberate modular int32 overflow). -fno-sanitize-recover makes every finding
# fatal so the fuzzer stops on the first one.
set(FUZZ_FLAGS "-g -fno-omit-frame-pointer -fsanitize=address,undefined")
set(FUZZ_FLAGS "${FUZZ_FLAGS} -fno-sanitize-recover=all")

if(FUZZ_USE_LIBFUZZER)
    set(FUZZ_FLAGS "${FUZZ_FLAGS} -fsanitize=fuzzer-no-link")
endif()

set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS} ${FUZZ_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FUZZ_FLAGS}")

# On macOS with Homebrew LLVM, link against LLVM's libc++ so the sanitizer
# runtime and the C++ library agree. The clang++ driver already adds -lc++
# implicitly; the -L/-rpath just steers it to LLVM's copy over the system one.
if(APPLE)
    get_filename_component(LLVM_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(LLVM_PREFIX  "${LLVM_BIN_DIR}" DIRECTORY)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${LLVM_PREFIX}/lib/c++ -Wl,-rpath,${LLVM_PREFIX}/lib/c++")
endif()

# Build the library from the repo root so its translation units pick up the
# sanitizer/coverage instrumentation set in CMAKE_CXX_FLAGS above.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. micro-wav-build)

# --- WAV streaming fuzzer ---------------------------------------------------
add_executable(fuzz_wav_decode fuzz_wav_decode.cpp)
target_link_libraries(fuzz_wav_decode PRIVATE micro_wav)

if(FUZZ_USE_LIBFUZZER)
    target_compile_options(fuzz_wav_decode PRIVATE -fsanitize=fuzzer)
    target_link_options(fuzz_wav_decode    PRIVATE -fsanitize=fuzzer)
else()
    target_compile_definitions(fuzz_wav_decode PRIVATE FUZZ_STANDALONE=1)
endif()
