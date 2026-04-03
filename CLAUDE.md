# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

microWAV is a byte-by-byte streaming WAV decoder for resource-constrained embedded devices. It's a single-header/single-source C++11 static library with zero dynamic allocation and no external dependencies.

## Build Commands

```bash
# Build (host)
cmake -B build -DENABLE_WERROR=ON && cmake --build build

# Lint (clang-tidy)
./script/clang-tidy.sh        # check
./script/clang-tidy.sh --fix  # auto-fix

# Pre-commit checks (formatting, markdown lint, etc.)
pre-commit run --all-files
```

Tests are built with `-DENABLE_TESTS=ON` and run via `ctest --test-dir build`.

## Architecture

The library is two files: `include/micro_wav/wav_decoder.h` (public API) and `src/wav_decoder.cpp` (implementation).

**`WAVDecoder`** exposes a single `decode()` function that handles both header parsing and audio decoding. Callers feed arbitrary-sized byte chunks via `decode()`, which returns `WAV_DECODER_NEED_MORE_DATA`, `WAV_DECODER_HEADER_READY`, `WAV_DECODER_SUCCESS`, `WAV_DECODER_END_OF_STREAM`, or an error. Internally, a 4-byte accumulator (`buf_[4]`) collects field data during header parsing and buffers partial audio samples during decoding. The header parser handles both standard and extended fmt chunks (WAVE_FORMAT_EXTENSIBLE) and auto-skips unknown chunks with RIFF-compliant even-byte alignment. Audio decoding supports PCM (8/16/24/32-bit), G.711 A-law/mu-law (decoded to 16-bit PCM), and IEEE float 32-bit (decoded to 32-bit integer PCM).

## Code Style

- Google C++ base style (clang-format), 4-space indent, 100-char column limit
- Private members suffixed with `_`
- No exceptions, no STL containers — enum return codes, raw arrays, bit-shifting for endianness
- Strict warnings: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`

## Build Targets

The CMake config auto-detects ESP-IDF vs host builds via `cmake/esp-idf.cmake` and `cmake/host.cmake`. Distribution supports ESP-IDF Component Manager, PlatformIO, and CMake subdirectory integration.
